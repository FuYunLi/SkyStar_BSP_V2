/**
 * @file app_main.c
 * @brief 应用层主入口实现
 * @note 注册 Demo + 启动 Task，无 while(1)
 */

#include "app_main.h"
#include "MultiTimer.h"
#include "demos/app_uart_demo.h"

/* ================================================================
 * 私有变量
 * ================================================================ */

static MultiTimer s_timer_led;

/* ================================================================
 * 定时器回调
 * ================================================================ */

static void led_blink_callback(MultiTimer* timer, void* userData)
{
    /* TODO: 添加 LED 翻转逻辑 */
    
    /* 重新启动，实现周期定时 */
    multiTimerStart(timer, 500, led_blink_callback, userData);
}

/* ================================================================
 * 应用初始化
 * ================================================================ */

void app_main_init(void)
{
    /* 启动 LED 闪烁定时器（500ms 周期） */
    multiTimerStart(&s_timer_led, 500, led_blink_callback, NULL);
    
    /* 启动串口测试验证模块 */
    app_uart_demo_init();
    
    /* 后续在此注册其他常驻任务 */
}
