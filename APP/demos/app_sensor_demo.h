/**
 * @file app_sensor_demo.h
 * @brief 温湿度传感器测试 Demo 头文件
 * @note 导出 Shell 命令初始化入口
 */

#ifndef __APP_SENSOR_DEMO_H
#define __APP_SENSOR_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化温湿度测试 Demo 并注册 Shell 指令
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_sensor_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SENSOR_DEMO_H */
