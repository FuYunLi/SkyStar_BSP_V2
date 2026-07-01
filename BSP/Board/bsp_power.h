/**
 * @file bsp_power.h
 * @brief 通用功率监测板级支持抽象接口
 * @note 隔离底层具体物理芯片，应用层通过此接口访问电源电压、电流和功率数据
 */

#ifndef __BSP_POWER_H
#define __BSP_POWER_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化板载功率监测芯片
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_power_init(void);

/**
 * @brief  读取当前的功率及电压电流数据
 * @param[out] voltage 存放电压结果的指针（单位：V）
 * @param[out] current 存放电流结果的指针（单位：A）
 * @param[out] power 存放功率结果的指针（单位：W）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_power_read(float *voltage, float *current, float *power);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_POWER_H */
