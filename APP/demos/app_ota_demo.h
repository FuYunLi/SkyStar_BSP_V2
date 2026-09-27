/**
 * @file app_ota_demo.h
 * @brief OTA 固件升级自检演示头文件
 * @note 对标 RocketPi mqtt_ota 的传输-落盘环节（UART Ymodem 版）。
 *       【未验证】OTA 闭环尚未上板实测。
 */

#ifndef __APP_OTA_DEMO_H
#define __APP_OTA_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 OTA 演示模块
 */
bsp_status_t app_ota_demo_init(void);

/**
 * @brief OTA 接收泵（须在 app_main_process 中周期调用）
 */
void app_ota_demo_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_OTA_DEMO_H */
