/**
 * @file port_i2s.c
 * @brief I2S 音频接口层实现
 * @note SPI2 与 I2S2 为同一外设：CubeMX 已将其初始化为 SPI 模式
 *       （W25Q128/ICM42688 使用），本模块以 I2S 模式接管外设，
 *       deinit 时通过 HAL_SPI_Init 恢复原 SPI 配置。
 *       【未验证】音频链路尚未上板实测。
 */

#include "port_i2s.h"
#include "stm32f4xx_hal.h"
#include "spi.h"

/* ================================================================
 * 私有变量
 * ================================================================ */

/* I2S2 自持句柄：与 CubeMX 的 hspi2 同外设不同句柄，互不污染配置 */
static I2S_HandleTypeDef s_i2s2_handle;

/* 发送 DMA 描述：SPI2_TX 固定挂 DMA1_Stream4 通道 0（参考手册 DMA 映射表） */
static DMA_HandleTypeDef s_i2s2_dma_tx;

/* 发送完成回调注册表（单实例单回调足够） */
static port_async_cb_t s_tx_cb;
static void *s_tx_ctx;

/* 发送忙标志，ISR 与主上下文共享 */
static volatile bool s_tx_busy;

/* ================================================================
 * HAL 回调函数
 * ================================================================ */

/**
 * @brief I2S 底层初始化（HAL_I2S_Init 内部调用）
 * @note 时钟、GPIO 复用与 DMA 均在此配置：
 *       PB10=I2S2_CK、PB9=I2S2_WS、PC3=I2S2_SD（AF5），
 *       PC6=I2S2_MCK（AF6），SW7 BIT3 需拨至 I2S2 位。
 */
void HAL_I2S_MspInit(I2S_HandleTypeDef *hi2s)
{
    GPIO_InitTypeDef gpio_init = {0};

    if (hi2s->Instance != SPI2)
    {
        return;
    }

    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* 四线复用推挽：CK/WS/SD 供 ES8388，MCLK 提供主时钟 */
    gpio_init.Pin = GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_3;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF5_SPI2;
    (void)HAL_GPIO_Init(GPIOB, &gpio_init);
    (void)HAL_GPIO_Init(GPIOC, &gpio_init);

    gpio_init.Pin = GPIO_PIN_6;
    gpio_init.Alternate = GPIO_AF6_SPI2;
    (void)HAL_GPIO_Init(GPIOC, &gpio_init);

    /* 发送 DMA：16 位半字对齐，普通模式（双缓冲由上层逐段重启） */
    s_i2s2_dma_tx.Instance = DMA1_Stream4;
    s_i2s2_dma_tx.Init.Channel = DMA_CHANNEL_0;
    s_i2s2_dma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    s_i2s2_dma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    s_i2s2_dma_tx.Init.MemInc = DMA_MINC_ENABLE;
    s_i2s2_dma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    s_i2s2_dma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    s_i2s2_dma_tx.Init.Mode = DMA_NORMAL;
    s_i2s2_dma_tx.Init.Priority = DMA_PRIORITY_HIGH;
    s_i2s2_dma_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    (void)HAL_DMA_Init(&s_i2s2_dma_tx);

    __HAL_LINKDMA(hi2s, hdmatx, s_i2s2_dma_tx);

    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
}

/**
 * @brief 重写 HAL 库 I2S 发送完成回调入口，分发至注册的业务回调
 */
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s != &s_i2s2_handle)
    {
        return;
    }

    s_tx_busy = false;

    if (s_tx_cb != NULL)
    {
        s_tx_cb((uint8_t)PORT_I2S_2, BSP_OK, s_tx_ctx);
    }
}

/**
 * @brief 重写 HAL 库 I2S 错误回调入口
 */
void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s != &s_i2s2_handle)
    {
        return;
    }

    s_tx_busy = false;

    if (s_tx_cb != NULL)
    {
        s_tx_cb((uint8_t)PORT_I2S_2, BSP_ERROR, s_tx_ctx);
    }
}

/* ================================================================
 * 中断入口
 * CubeMX 未为 SPI2_TX 配置 DMA 中断，DMA1_Stream4 入口由本模块补齐。
 * 若后续在 CubeMX 中使能 SPI2 的 DMA，须删除此定义避免重复链接。
 * ================================================================ */

/**
 * @brief DMA1 Stream4 中断入口（I2S2 发送 DMA）
 */
void DMA1_Stream4_IRQHandler(void)
{
    (void)HAL_DMA_IRQHandler(s_i2s2_handle.hdmatx);
}

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 获取 I2S 逻辑通道句柄，未初始化返回 NULL
 */
static I2S_HandleTypeDef *s_i2s_get_ctx(port_i2s_id_t id)
{
    if (id >= PORT_I2S_MAX)
    {
        return NULL;
    }

    if (s_i2s2_handle.Instance != SPI2)
    {
        return NULL;
    }

    return &s_i2s2_handle;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 I2S 接口（主发送、飞利浦 16 位、MCLK 输出）
 */
bsp_status_t port_i2s_init(port_i2s_id_t id, uint32_t sample_rate_hz)
{
    if (id >= PORT_I2S_MAX)
    {
        return BSP_EINVAL;
    }

    if ((sample_rate_hz < 8000U) || (sample_rate_hz > 48000U))
    {
        return BSP_EINVAL;
    }

    RCC_PeriphCLKInitTypeDef periph_clk = {0};

    /* I2S 内核时钟走 PLLI2S：VCO 输入 = HSE/PLLM = 8/4 = 2MHz（与主 PLL
     * 共享 M 分频），N=216/R=5 → 86.4MHz。该频率下 48kHz 误差约 +0.45%，
     * 44.1kHz 约 +2.0%，其余常用采样率 ≤2%；如需精确无级时钟可再调 N/R */
    periph_clk.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    periph_clk.PLLI2S.PLLI2SN = 216U;
    periph_clk.PLLI2S.PLLI2SR = 5U;
    if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK)
    {
        return BSP_ERROR;
    }

    s_i2s2_handle.Instance = SPI2;
    s_i2s2_handle.Init.Mode = I2S_MODE_MASTER_TX;
    s_i2s2_handle.Init.Standard = I2S_STANDARD_PHILIPS;
    s_i2s2_handle.Init.DataFormat = I2S_DATAFORMAT_16B;
    s_i2s2_handle.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    s_i2s2_handle.Init.AudioFreq = sample_rate_hz;
    s_i2s2_handle.Init.CPOL = I2S_CPOL_LOW;
    s_i2s2_handle.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;

    if (HAL_I2S_Init(&s_i2s2_handle) != HAL_OK)
    {
        return BSP_ERROR;
    }

    s_tx_busy = false;
    s_tx_cb = NULL;
    s_tx_ctx = NULL;

    return BSP_OK;
}

/**
 * @brief 反初始化 I2S 并恢复 SPI2 模式
 */
bsp_status_t port_i2s_deinit(port_i2s_id_t id)
{
    if (id >= PORT_I2S_MAX)
    {
        return BSP_EINVAL;
    }

    if (s_i2s2_handle.Instance != SPI2)
    {
        return BSP_OK;
    }

    (void)HAL_I2S_DMAStop(&s_i2s2_handle);
    s_tx_busy = false;
    s_tx_cb = NULL;
    s_tx_ctx = NULL;

    if (HAL_I2S_DeInit(&s_i2s2_handle) != HAL_OK)
    {
        return BSP_ERROR;
    }

    s_i2s2_handle.Instance = NULL;

    /* 恢复 CubeMX 保存的 SPI2 配置：hspi2.Init 在上电时已由
     * MX_SPI2_Init 填充，HAL_SPI_Init 会经 MspInit 重配 GPIO 与 DMA，
     * W25Q/IMU 随即可用 */
    if (HAL_SPI_Init(&hspi2) != HAL_OK)
    {
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief 以 DMA 方式异步发送一组采样
 */
bsp_status_t port_i2s_write_dma(port_i2s_id_t id, const uint16_t *samples, uint16_t count,
                                port_async_cb_t cb, void *user_ctx)
{
    I2S_HandleTypeDef *hi2s = s_i2s_get_ctx(id);

    if ((hi2s == NULL) || (samples == NULL) || (count == 0U))
    {
        return BSP_EINVAL;
    }

    if (s_tx_busy)
    {
        return BSP_BUSY;
    }

    s_tx_cb = cb;
    s_tx_ctx = user_ctx;
    s_tx_busy = true;

    if (HAL_I2S_Transmit_DMA(hi2s, (uint16_t *)samples, count) != HAL_OK)
    {
        s_tx_busy = false;
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief 查询发送通道是否忙碌
 */
bool port_i2s_is_busy(port_i2s_id_t id)
{
    I2S_HandleTypeDef *hi2s = s_i2s_get_ctx(id);

    if (hi2s == NULL)
    {
        return false;
    }

    return s_tx_busy || (hi2s->State == HAL_I2S_STATE_BUSY_TX);
}

/**
 * @brief 停止 DMA 发送并关闭 I2S 输出
 */
bsp_status_t port_i2s_stop(port_i2s_id_t id)
{
    I2S_HandleTypeDef *hi2s = s_i2s_get_ctx(id);

    if (hi2s == NULL)
    {
        return BSP_EINVAL;
    }

    (void)HAL_I2S_DMAStop(hi2s);
    s_tx_busy = false;
    s_tx_cb = NULL;
    s_tx_ctx = NULL;

    return BSP_OK;
}
