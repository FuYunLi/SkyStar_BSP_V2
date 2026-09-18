/**
 * @file app_cjson_demo.h
 * @brief cJSON 协议控制自检演示头文件
 * @note 对标 RocketPi 09_rocketpi_uart_control_led_cjson，演示 JSON 协议解析、
 *       类型校验与设备控制的完整链路。
 */

#ifndef __APP_CJSON_DEMO_H
#define __APP_CJSON_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 cJSON 协议控制演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_cjson_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CJSON_DEMO_H */
