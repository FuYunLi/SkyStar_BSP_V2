/**
 * @file dev_potentiometer.c
 * @brief 旋转电位器设备驱动实现源文件
 * @note 内置一阶低通滤波（IIR）并对极限电位边界进行截断与插值，确保读数稳定。
 */

#include "dev_potentiometer.h"
#include "port_adc.h"

/* ================================================================
 * 宏定义与私有静态变量
 * ================================================================ */

/* 滤波系数 alpha，越小越平滑但响应变慢，取 0.2 */
#define POTENTIOMETER_FILTER_ALPHA   (0.2f)

/* 电位器极值微调，用于在物理极限两端能够平稳地读出 0% 与 100% */
#define POTENTIOMETER_MIN_VOLTAGE    (10U)    /* 小于 10mV 视为 0% */
#define POTENTIOMETER_MAX_VOLTAGE    (3280U)  /* 大于 3280mV 视为 100% */

static bool s_is_init = false;               /* 设备驱动初始化状态 */
static float s_filtered_voltage = 0.0f;      /* 内部滤波电压值缓存 */

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化旋转电位器设备驱动
 */
bsp_status_t dev_potentiometer_init(void)
{
    bsp_status_t status = port_adc_init();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 首次初始化时，做一次原始值获取作为滤波器初始状态 */
    uint32_t initial_vol = 0;
    status = port_adc_read_voltage(PORT_ADC_CH_POTENTIOMETER, &initial_vol);
    if (status != BSP_OK)
    {
        s_filtered_voltage = 0.0f;
    }
    else
    {
        s_filtered_voltage = (float)initial_vol;
    }

    s_is_init = true;
    return BSP_OK;
}

/**
 * @brief 获取滤波后的电位器电压值（单位：mV）
 */
bsp_status_t dev_potentiometer_get_voltage(uint32_t *voltage_mv)
{
    if (voltage_mv == NULL)
    {
        return BSP_EINVAL;
    }
    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    uint32_t cur_vol = 0;
    bsp_status_t status = port_adc_read_voltage(PORT_ADC_CH_POTENTIOMETER, &cur_vol);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 一阶低通滤波 IIR */
    s_filtered_voltage = (POTENTIOMETER_FILTER_ALPHA * (float)cur_vol) + 
                         ((1.0f - POTENTIOMETER_FILTER_ALPHA) * s_filtered_voltage);

    *voltage_mv = (uint32_t)s_filtered_voltage;

    return BSP_OK;
}

/**
 * @brief 获取滤波后的电位器旋转百分比（0.0% ~ 100.0%）
 */
bsp_status_t dev_potentiometer_get_percent(float *percent)
{
    if (percent == NULL)
    {
        return BSP_EINVAL;
    }
    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    uint32_t vol_mv = 0;
    bsp_status_t status = dev_potentiometer_get_voltage(&vol_mv);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 边界截断与插值映射 */
    if (vol_mv <= POTENTIOMETER_MIN_VOLTAGE)
    {
        *percent = 0.0f;
    }
    else if (vol_mv >= POTENTIOMETER_MAX_VOLTAGE)
    {
        *percent = 100.0f;
    }
    else
    {
        /* 线性映射换算 */
        float range = (float)(POTENTIOMETER_MAX_VOLTAGE - POTENTIOMETER_MIN_VOLTAGE);
        float offset = (float)(vol_mv - POTENTIOMETER_MIN_VOLTAGE);
        *percent = (offset / range) * 100.0f;
    }

    return BSP_OK;
}
