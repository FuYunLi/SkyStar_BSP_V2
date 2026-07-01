/**
 * @file bsp_sensor.c
 * @brief 通用温湿度传感器板级支持抽象层实现
 * @note 桥接 LibDriver AHT20 驱动示例
 */

#include "bsp_sensor.h"
#include "driver_aht20_basic.h"

/**
 * @brief  初始化温湿度环境传感器
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_sensor_init(void)
{
    uint8_t status = aht20_basic_init();
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
 * @brief  读取当前的温湿度环境数据
 * @param[out] temperature 存放温度结果的指针（单位：摄氏度）
 * @param[out] humidity 存放湿度结果的指针（单位：%RH）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_sensor_read_environmental(float *temperature, float *humidity)
{
    BSP_CHECK_NULL(temperature);
    BSP_CHECK_NULL(humidity);

    uint8_t hum_pct = 0;
    uint8_t status = aht20_basic_read(temperature, &hum_pct);
    if (status == 0)
    {
        *humidity = (float)hum_pct;
        return BSP_OK;
    }
    else
    {
        return BSP_ERROR;
    }
}
