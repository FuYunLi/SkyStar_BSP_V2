/**
 * @file port_adc.c
 * @brief 内置 ADC 接口层实现源文件
 * @note 采用动态重配置通道方式，确保多通道轮询采样时的准确性与通道间隔离。
 */

#include "port_adc.h"
#include "stm32f4xx_hal.h"
#include "adc.h"

/* ================================================================
 * 宏定义与外部变量声明
 * ================================================================ */

/* 外部已由 CubeMX 声明的 ADC1 控制句柄 */
extern ADC_HandleTypeDef hadc1;

/* 内部逻辑通道到 STM32 物理通道的映射表 */
static const uint32_t s_adc_ch_map[PORT_ADC_CH_MAX] = {
    [PORT_ADC_CH_POTENTIOMETER] = ADC_CHANNEL_10,
};

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化内置 ADC 接口层
 */
bsp_status_t port_adc_init(void)
{
    /* 由于 CubeMX 已在 main.c 中执行 MX_ADC1_Init，此处仅需检查句柄 */
    if (hadc1.Instance != ADC1)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}

/**
 * @brief 读取指定通道的 ADC 原始 LSB 数值
 */
bsp_status_t port_adc_read_raw(port_adc_ch_t ch, uint32_t *raw_value)
{
    if (ch >= PORT_ADC_CH_MAX)
    {
        return BSP_EINVAL;
    }
    if (raw_value == NULL)
    {
        return BSP_EINVAL;
    }

    /* 动态重配通道：将当前逻辑通道关联到 Rank 1 */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = s_adc_ch_map[ch];
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
    
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
        return BSP_ERROR;
    }

    /* 开启 ADC 转换 */
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return BSP_ERROR;
    }

    /* 轮询等待转换完成，超时设为 10ms */
    HAL_StatusTypeDef status = HAL_ADC_PollForConversion(&hadc1, 10U);
    if (status != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return (status == HAL_TIMEOUT) ? BSP_ETIMEOUT : BSP_ERROR;
    }

    *raw_value = HAL_ADC_GetValue(&hadc1);

    /* 停止转换以使通道状态干净 */
    (void)HAL_ADC_Stop(&hadc1);

    return BSP_OK;
}

/**
 * @brief 读取指定通道换算后的 mV 电压值
 */
bsp_status_t port_adc_read_voltage(port_adc_ch_t ch, uint32_t *voltage_mv)
{
    if (voltage_mv == NULL)
    {
        return BSP_EINVAL;
    }

    uint32_t raw = 0;
    bsp_status_t status = port_adc_read_raw(ch, &raw);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 电压换算公式：V = (raw * VREF) / MAX_LSB */
    *voltage_mv = (raw * PORT_ADC_VREF_MV) / PORT_ADC_MAX_LSB;

    return BSP_OK;
}
