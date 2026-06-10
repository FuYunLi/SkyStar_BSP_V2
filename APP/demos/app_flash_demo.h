/**
 * @file app_flash_demo.h
 * @brief Flash 与 LittleFS 文件系统演示与自检模块头文件
 * @note 负责初始化挂载 LittleFS 并注册相关 Shell 指令用于自测。
 */

#ifndef __APP_FLASH_DEMO_H
#define __APP_FLASH_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

bsp_status_t app_flash_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_FLASH_DEMO_H */
