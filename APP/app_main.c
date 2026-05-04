/**
 * @file app_main.c
 * @brief 应用层主入口实现
 * @note 注册 Demo + 启动 Task，无 while(1)
 */

#include "app_main.h"
#include "MultiTimer.h"

/* ==================== 示例定时器 ==================== */

/* 定时器结构体（静态分配） */
static MultiTimer s_timer_led;

/* LED 闪烁回调（示例） */
static void led_blink_callback(MultiTimer* timer, void* userData)
{
    /* 周期定时器：在回调中重新启动 */
    /* TODO: 添加 LED 翻转逻辑 */
    
    /* 重新启动，实现周期定时 */
    multiTimerStart(timer, 500, led_blink_callback, userData);
}

/* ==================== 应用初始化 ==================== */

void app_main_init(void)
{
    /* 启动 LED 闪烁定时器（500ms 周期） */
    multiTimerStart(&s_timer_led, 500, led_blink_callback, NULL);
    
    /* 后续在此注册其他常驻任务 */
    
    /* Demo 通过 Shell 命令触发，不自动运行 */
}
