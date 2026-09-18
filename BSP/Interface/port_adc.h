/**
 * @file port_adc.h
 * @brief 内置 ADC 接口层头文件
 * @note 封装片上内置 ADC 的转换控制，提供统一的 LSB 及毫伏级电压读取抽象。
 */

#ifndef __PORT_ADC_H
#define __PORT_ADC_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* ADC 基准电压宏定义，单位 mV */
#define PORT_ADC_VREF_MV       (3300U)
/* ADC 最大分辨率对应 LSB (12位分辨率) */
#define PORT_ADC_MAX_LSB       (4095U)

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 内置 ADC 通道标识枚举
 */
typedef enum
{
    PORT_ADC_CH_POTENTIOMETER = 0,  /* PC0 / ADC1_IN10 旋转电位器 */
    PORT_ADC_CH_MCU_TEMP,           /* ADC1_IN16 片内温度传感器 */
    PORT_ADC_CH_MAX
} port_adc_ch_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化内置 ADC 接口层
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_ERROR 句柄未初始化或配置失败
 */
bsp_status_t port_adc_init(void);

/**
 * @brief 读取指定通道的 ADC 原始 LSB 数值
 * @param[in] ch ADC 逻辑通道 ID
 * @param[out] raw_value 存储读取到的原始数据 LSB 指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 转换失败
 *         - BSP_ETIMEOUT 采样超时
 */
bsp_status_t port_adc_read_raw(port_adc_ch_t ch, uint32_t *raw_value);

/**
 * @brief 读取指定通道换算后的 mV 电压值
 * @param[in] ch ADC 逻辑通道 ID
 * @param[out] voltage_mv 存储转换后的毫伏电压指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 采样失败
 */
bsp_status_t port_adc_read_voltage(port_adc_ch_t ch, uint32_t *voltage_mv);

/**
 * @brief 读取片内温度传感器换算后的温度值
 * @note 数据手册参数：V25 = 0.76V，平均斜率 2.5mV/°C。
 *       温度 = ((Vsense - V25) / Slope) + 25。
 * @param[out] temp_deci_c 存储温度的指针，单位 0.1°C（如 253 表示 25.3°C）
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 采样失败
 */
bsp_status_t port_adc_read_temperature(int16_t *temp_deci_c);

/**
 * @brief 反初始化内置 ADC 接口层
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_ERROR 配置释放失败
 */
bsp_status_t port_adc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_ADC_H */
