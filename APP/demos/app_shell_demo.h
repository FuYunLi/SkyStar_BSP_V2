/**
 * @file app_shell_demo.h
 * @brief Letter Shell 使用演示层头文件
 * @note 提供 Demo 的初始化接口声明。
 */

#ifndef APP_SHELL_DEMO_H
#define APP_SHELL_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Letter Shell 演示模块
 * @return bsp_status_t 初始化状态，成功返回 BSP_OK
 */
bsp_status_t app_shell_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SHELL_DEMO_H */
