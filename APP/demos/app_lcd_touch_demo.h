/**
 * @file app_lcd_touch_demo.h
 * @brief 屏幕划线与触摸反馈综合测试 Demo 头文件
 * @note 基于 LVGL Canvas 实现，画线逻辑由 LVGL 输入设备回调驱动，
 *       无需主循环额外轮询。
 */

#ifndef __APP_LCD_TOUCH_DEMO_H
#define __APP_LCD_TOUCH_DEMO_H

#include "bsp_board.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化屏幕划线与触摸反馈 Demo，导出 Shell 命令
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_lcd_touch_demo_init(void);

/**
 * @brief 查询画线模式是否正在运行
 * @retval true 画线模式激活中
 * @retval false 画线模式未激活
 */
bool app_lcd_touch_demo_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_LCD_TOUCH_DEMO_H */
