/**
 * @file port_i2s.c
 * @brief I2S 物理层接口实现
 */

#include "port_i2s.h"
#include "stm32f4xx_hal.h"
#include "bsp_logger.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "PORT_I2S"

/* I2S 及 DMA 全局句柄 */
I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_i2s2_tx;

/* 中断回调函数指针 */
static port_i2s_cb_t s_half_cb = NULL;
static port_i2s_cb_t s_cplt_cb = NULL;

/**
 * @brief  初始化 I2S 物理外设及其 DMA、引脚和音频时钟
 */
bsp_status_t port_i2s_init(void)
{
    /* 1. 配置 I2S2 专属时钟源 PLLI2S */
    /* HSE = 8MHz, PLLM = 4, HSE/PLLM = 2MHz */
    /* I2S_CLK = (HSE/PLLM) * PLLI2SN / PLLI2SR = 2MHz * 158 / 2 = 158MHz */
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
    PeriphClkInitStruct.PLLI2S.PLLI2SN = 158;
    PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        log_e("PLLI2S Clock Configuration failed");
        return BSP_ERROR;
    }

    /* 2. 使能相关外设及 GPIO 时钟 */
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* 3. 配置 I2S2 物理管脚复用 */
    /* PB9  -> I2S2_WS (AF5) */
    /* PB10 -> I2S2_CK (AF5) */
    /* PC3  -> I2S2_SD (AF5) */
    /* PC6  -> I2S2_MCK (AF5) */
    /* PC2  -> I2S2_ext_SD (AF6) */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_6;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Alternate = GPIO_AF6_I2S2ext;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* 4. 配置 DMA1 Stream 4 Channel 0 用于 I2S2_TX 发送 */
    hdma_i2s2_tx.Instance = DMA1_Stream4;
    hdma_i2s2_tx.Init.Channel = DMA_CHANNEL_0;
    hdma_i2s2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_i2s2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_i2s2_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_i2s2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_i2s2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_i2s2_tx.Init.Mode = DMA_CIRCULAR;
    hdma_i2s2_tx.Init.Priority = DMA_PRIORITY_HIGH;
    hdma_i2s2_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_i2s2_tx) != HAL_OK)
    {
        log_e("DMA1 Stream4 initialization failed");
        return BSP_ERROR;
    }

    /* 关联 DMA 句柄与 I2S 句柄 */
    __HAL_LINKDMA(&hi2s2, hdmatx, hdma_i2s2_tx);

    /* 配置 DMA 中断优先级并使能中断 */
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

    /* 5. 初始化 I2S2 外设配置 */
    hi2s2.Instance = SPI2;
    hi2s2.Init.Mode = I2S_MODE_MASTER_TX;
    hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
    hi2s2.Init.DataFormat = I2S_DATAFORMAT_16B;
    hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
    hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_44K;
    hi2s2.Init.CPOL = I2S_CPOL_LOW;
    hi2s2.Init.ClockSource = I2S_CLOCK_PLL;

    if (HAL_I2S_Init(&hi2s2) != HAL_OK)
    {
        log_e("I2S2 initialization failed");
        return BSP_ERROR;
    }

    log_i("I2S2 physical peripheral & DMA stream initialized successfully");
    return BSP_OK;
}

/**
 * @brief  动态设置 I2S 采样率
 */
bsp_status_t port_i2s_set_sample_rate(uint32_t sample_rate)
{
    if (hi2s2.Instance == NULL)
    {
        return BSP_ENODEV;
    }

    hi2s2.Init.AudioFreq = sample_rate;
    if (HAL_I2S_Init(&hi2s2) != HAL_OK)
    {
        log_e("Failed to set I2S sample rate: %d", (int)sample_rate);
        return BSP_ERROR;
    }

    log_i("I2S sample rate successfully set to %d Hz", (int)sample_rate);
    return BSP_OK;
}


/**
 * @brief  释放 I2S 物理外设并关闭 DMA 发送
 */
bsp_status_t port_i2s_deinit(void)
{
    /* 停止 DMA */
    (void)HAL_I2S_DMAStop(&hi2s2);

    /* 关闭 DMA 中断 */
    HAL_NVIC_DisableIRQ(DMA1_Stream4_IRQn);

    /* 反初始化 DMA */
    (void)HAL_DMA_DeInit(&hdma_i2s2_tx);

    /* 反初始化 I2S */
    (void)HAL_I2S_DeInit(&hi2s2);

    /* 释放引脚，交回默认复位状态 */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_9 | GPIO_PIN_10);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6);

    log_i("I2S2 peripheral deinitialized and pins released");
    return BSP_OK;
}

/**
 * @brief  注册音频传输半完成与全完成的中断回调函数
 */
void port_i2s_register_callbacks(port_i2s_cb_t half_cb, port_i2s_cb_t cplt_cb)
{
    s_half_cb = half_cb;
    s_cplt_cb = cplt_cb;
}

/**
 * @brief  开始 I2S DMA 双缓冲推流发送
 */
bsp_status_t port_i2s_write_dma(uint16_t *buffer, uint16_t len)
{
    if (buffer == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    HAL_StatusTypeDef status = HAL_I2S_Transmit_DMA(&hi2s2, buffer, len);
    return hal_to_bsp_status(status);
}

/**
 * @brief  停止 I2S DMA 音频传输
 */
bsp_status_t port_i2s_stop(void)
{
    HAL_StatusTypeDef status = HAL_I2S_DMAStop(&hi2s2);
    return hal_to_bsp_status(status);
}

/**
 * @brief  检查 I2S 是否处于忙（传输中）状态
 */
bool port_i2s_is_busy(void)
{
    return (HAL_I2S_GetState(&hi2s2) == HAL_I2S_STATE_BUSY_TX);
}

/* ================================================================
 * HAL I2S 中断回调重写
 * ================================================================ */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI2 && s_half_cb != NULL)
    {
        s_half_cb();
    }
}

void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s->Instance == SPI2 && s_cplt_cb != NULL)
    {
        s_cplt_cb();
    }
}
