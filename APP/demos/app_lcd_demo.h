/**
 * @file app_lcd_demo.h
 * @brief LCD 演示与自检测试模块头文件
 */

#ifndef __APP_LCD_DEMO_H
#define __APP_LCD_DEMO_H

#include "bsp_board.h"

/**
 * @brief 初始化 LCD 演示模块，导出自检命令至 Shell
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_lcd_demo_init(void);

#endif /* __APP_LCD_DEMO_H */
