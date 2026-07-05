/**
 * @file app_ymodem_demo.h
 * @brief Ymodem 文件接收应用演示头文件
 */

#ifndef APP_YMODEM_DEMO_H
#define APP_YMODEM_DEMO_H

#include "bsp_board.h"
#include <stdbool.h>

/* Ymodem 传输路由重定向全局使能标志 */
extern volatile bool g_ymodem_active;

/**
 * @brief 初始化 Ymodem 演示应用
 * @return bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_ymodem_demo_init(void);

/**
 * @brief Ymodem 演示轮询处理任务
 * @note 放置在主循环中以非阻塞形式轮询并驱动状态机
 */
void app_ymodem_demo_process(void);

#endif /* APP_YMODEM_DEMO_H */
