/**
 * @file bsp_power.c
 * @brief 通用功率监测板级支持抽象层实现
 * @note 桥接 LibDriver INA226 驱动示例
 */

#include "bsp_power.h"
#include "driver_ina226_basic.h"

/* 标定主板分流电阻阻值为 15mΩ (即 0.015 欧姆) */
#define BSP_POWER_SHUNT_RESISTOR_OHM  0.015

/**
 * @brief  初始化板载功率监测芯片
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_power_init(void)
{
    /* 调用 INA226 示例初始化，使用默认地址引脚接地 (0x40) 及 15mΩ 采样电阻 */
    uint8_t status = ina226_basic_init(INA226_ADDRESS_0, BSP_POWER_SHUNT_RESISTOR_OHM);
    if (status == 0)
    {
        return BSP_OK;
    }
    else
    {
        return BSP_ERROR;
    }
}

/**
 * @brief  读取当前的功率及电压电流数据
 * @param[out] voltage 存放电压结果的指针（单位：V）
 * @param[out] current 存放电流结果的指针（单位：A）
 * @param[out] power 存放功率结果的指针（单位：W）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_power_read(float *voltage, float *current, float *power)
{
    BSP_CHECK_NULL(voltage);
    BSP_CHECK_NULL(current);
    BSP_CHECK_NULL(power);

    float mv = 0.0f;
    float ma = 0.0f;
    float mw = 0.0f;

    /* 从示例中读取原始物理单位 (mV, mA, mW) */
    uint8_t status = ina226_basic_read(&mv, &ma, &mw);
    if (status == 0)
    {
        /* 转换为标准应用物理单位 (V, A, W) */
        *voltage = mv / 1000.0f;
        *current = ma / 1000.0f;
        *power   = mw / 1000.0f;
        return BSP_OK;
    }
    else
    {
        return BSP_ERROR;
    }
}
