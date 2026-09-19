/**
 * @file port_i2s.c
 * @brief I2S 物理层抽象接口实现
 * @note  I2S2 复用 SPI2 外设实例，CubeMX 无法同时生成两种模式的初始化代码，
 *        因此本层自持 I2S 句柄，在 init 时运行时配置 PLLI2S 时钟、GPIO 复用与
 *        循环 DMA。模式切换前的 SPI 反初始化由 bsp_bus 仲裁器保证。
 */

#include "port_i2s.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* PLLI2S 参数：VCO 输入 1MHz（HSE 8MHz / PLLM 8），N=271 R=6 → 45.167MHz，
 * 对 44.1kHz 家族（256*fs = 11.2896MHz）偏差 < 0.1%，为 44.1k 标准配置 */
#define PORT_I2S_PLLI2S_N   (271U)
#define PORT_I2S_PLLI2S_R   (6U)

/* I2S2 GPIO 复用配置（AF5 = SPI2/I2S2） */
#define PORT_I2S_AF         (GPIO_AF5_SPI2)

/* ================================================================
 * 私有类型
 * ================================================================ */

/* 流式发送上下文 */
typedef struct
{
    port_i2s_cb_t half_cb;
    port_i2s_cb_t full_cb;
    void          *user_ctx;
    volatile bool is_streaming;
} port_i2s_context_t;

/* ================================================================
 * 私有变量
 * ================================================================ */

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_i2s2_tx;
static port_i2s_context_t s_i2s_contexts[PORT_I2S_MAX];

static I2S_HandleTypeDef *hw_mapping[] = {
    [PORT_I2S_1] = &hi2s2,
};

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

static I2S_HandleTypeDef *get_hw(port_i2s_id_t id)
{
    if (id >= PORT_I2S_MAX)
    {
        return NULL;
    }
    return hw_mapping[id];
}

/**
 * @brief 配置 PLLI2S 时钟源
 */
static bsp_status_t i2s_clock_config(void)
{
    RCC_PeriphCLKInitTypeDef clk_init = {0};
    clk_init.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    clk_init.PLLI2S.PLLI2SN = PORT_I2S_PLLI2S_N;
    clk_init.PLLI2S.PLLI2SR = PORT_I2S_PLLI2S_R;
    return hal_to_bsp_status(HAL_RCCEx_PeriphCLKConfig(&clk_init));
}

/**
 * @brief 配置 I2S2 引脚复用（PB9 WS / PB10 CK / PC3 SD / PC6 MCK）
 */
static bsp_status_t i2s_gpio_config(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = PORT_I2S_AF;

    /* PB9 = I2S2_WS, PB10 = I2S2_CK */
    gpio_init.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* PC3 = I2S2_SD, PC6 = I2S2_MCK */
    gpio_init.Pin = GPIO_PIN_3 | GPIO_PIN_6;
    HAL_GPIO_Init(GPIOC, &gpio_init);

    return BSP_OK;
}

/**
 * @brief 配置 I2S2 发送循环 DMA（DMA1 Stream4 Channel0）
 */
static bsp_status_t i2s_dma_config(I2S_HandleTypeDef *hi2s)
{
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_i2s2_tx.Instance                 = DMA1_Stream4;
    hdma_i2s2_tx.Init.Channel             = DMA_CHANNEL_0;
    hdma_i2s2_tx.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_i2s2_tx.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_i2s2_tx.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_i2s2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_i2s2_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_i2s2_tx.Init.Mode                = DMA_CIRCULAR;
    hdma_i2s2_tx.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_i2s2_tx.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_i2s2_tx) != HAL_OK)
    {
        return BSP_ERROR;
    }

    __HAL_LINKDMA(hi2s, hdmatx, hdma_i2s2_tx);

    /* DMA 流中断与 SPI2 全局中断（I2S 错误通道） */
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    HAL_NVIC_SetPriority(SPI2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SPI2_IRQn);

    return BSP_OK;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t port_i2s_init(port_i2s_id_t id)
{
    I2S_HandleTypeDef *hi2s = get_hw(id);
    if (hi2s == NULL)
    {
        return BSP_EINVAL;
    }

    if (hi2s->State != HAL_I2S_STATE_RESET)
    {
        /* 已初始化则幂等返回 */
        return BSP_OK;
    }

    memset(&s_i2s_contexts[id], 0, sizeof(port_i2s_context_t));

    bsp_status_t status = i2s_clock_config();
    if (status != BSP_OK)
    {
        return status;
    }

    status = i2s_gpio_config();
    if (status != BSP_OK)
    {
        return status;
    }

    __HAL_RCC_SPI2_CLK_ENABLE();

    hi2s->Instance               = SPI2;
    hi2s->Init.Mode              = I2S_MODE_MASTER_TX;
    hi2s->Init.Standard          = I2S_STANDARD_PHILIPS;
    hi2s->Init.DataFormat        = I2S_DATAFORMAT_16B;
    hi2s->Init.MCLKOutput        = I2S_MCLKOUTPUT_ENABLE;
    hi2s->Init.AudioFreq         = I2S_AUDIOFREQ_44K;
    hi2s->Init.CPOL              = I2S_CPOL_LOW;
    hi2s->Init.FullDuplexMode    = I2S_FULLDUPLEXMODE_DISABLE;

    status = hal_to_bsp_status(HAL_I2S_Init(hi2s));
    if (status != BSP_OK)
    {
        return status;
    }

    return i2s_dma_config(hi2s);
}

bsp_status_t port_i2s_deinit(port_i2s_id_t id)
{
    I2S_HandleTypeDef *hi2s = get_hw(id);
    if (hi2s == NULL)
    {
        return BSP_EINVAL;
    }

    if (hi2s->State != HAL_I2S_STATE_RESET)
    {
        (void)port_i2s_stop(id);
        if (hal_to_bsp_status(HAL_I2S_DeInit(hi2s)) != BSP_OK)
        {
            return BSP_ERROR;
        }
    }
    (void)HAL_DMA_DeInit(&hdma_i2s2_tx);
    memset(&s_i2s_contexts[id], 0, sizeof(port_i2s_context_t));
    return BSP_OK;
}

bsp_status_t port_i2s_start_dma(port_i2s_id_t id, const uint16_t *buf,
                                size_t samples,
                                port_i2s_cb_t half_cb, port_i2s_cb_t full_cb,
                                void *user_ctx)
{
    I2S_HandleTypeDef *hi2s = get_hw(id);
    if (hi2s == NULL || buf == NULL || samples == 0U || (samples & 1U) != 0U)
    {
        return BSP_EINVAL;
    }

    if (hi2s->State == HAL_I2S_STATE_RESET)
    {
        return BSP_ERROR;
    }

    s_i2s_contexts[id].half_cb      = half_cb;
    s_i2s_contexts[id].full_cb      = full_cb;
    s_i2s_contexts[id].user_ctx     = user_ctx;
    s_i2s_contexts[id].is_streaming = true;

    return hal_to_bsp_status(HAL_I2S_Transmit_DMA(hi2s, (uint16_t *)buf, (uint16_t)samples));
}

bsp_status_t port_i2s_stop(port_i2s_id_t id)
{
    I2S_HandleTypeDef *hi2s = get_hw(id);
    if (hi2s == NULL)
    {
        return BSP_EINVAL;
    }

    s_i2s_contexts[id].is_streaming = false;
    s_i2s_contexts[id].half_cb = NULL;
    s_i2s_contexts[id].full_cb = NULL;

    if (hi2s->State != HAL_I2S_STATE_RESET)
    {
        return hal_to_bsp_status(HAL_I2S_DMAStop(hi2s));
    }
    return BSP_OK;
}

/* ================================================================
 * HAL 回调路由（按句柄归属派发到注册回调）
 * ================================================================ */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == hw_mapping[PORT_I2S_1])
    {
        port_i2s_context_t *ctx = &s_i2s_contexts[PORT_I2S_1];
        if (ctx->is_streaming && ctx->half_cb != NULL)
        {
            ctx->half_cb(ctx->user_ctx);
        }
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s == hw_mapping[PORT_I2S_1])
    {
        port_i2s_context_t *ctx = &s_i2s_contexts[PORT_I2S_1];
        if (ctx->is_streaming && ctx->full_cb != NULL)
        {
            ctx->full_cb(ctx->user_ctx);
        }
    }
}
