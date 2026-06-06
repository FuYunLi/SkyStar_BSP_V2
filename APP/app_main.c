/**
 * @file app_main.c
 * @brief 应用层主入口实现
 * @note 注册 Demo + 启动 Task，无 while(1)
 */

#include "app_main.h"
#include "MultiTimer.h"
#include "bsp_uart.h"
#include "port_dwt.h"
#include "port_gpio.h"
#include "dev_led.h"
#include "dev_buzzer.h"
#include "dev_ws2812.h"
#include "bsp_shell.h"

#define LOG_TAG "APP_MAIN"
#include "bsp_logger.h"
#include "demos/app_uart_demo.h"
#include "demos/app_shell_demo.h"
#include "demos/app_rgb_demo.h"

#if 0
static MultiTimer s_timer_log;
static uint8_t    s_log_test_count = 0U;
static void       log_test_callback(MultiTimer *timer, void *userData)
{
    log_w("log test count=%d", s_log_test_count++);
    /* 重新启动，实现周期定时 */
    multiTimerStart(timer, 500, log_test_callback, userData);
}
#endif

/* ================================================================
 * 应用初始化
 * ================================================================ */

void app_main_init(void)
{
    /* 初始化板级串口 */
    bsp_uart_init();

    /* 初始化 DWT 精确周期计数器 */
    port_dwt_init();

    /* 初始化逻辑 GPIO 引脚映射与安全时钟 */
    port_gpio_init();

    /* 初始化 LED、无源蜂鸣器与 WS2812 驱动 */
    dev_led_init();
    dev_buzzer_init();
    dev_ws2812_init();

    /* 初始化日志服务并打印测试日志 */
    bsp_logger_init();
    log_i("Hello world");
    log_d("Hello world");
    log_w("Hello world");
    log_e("Hello world");
    log_a("Hello world");

    __disable_irq();
    log_i("This log will trigger drop-logic because IRQ is disabled.");
    __enable_irq();

    /* 最后初始化板级 Shell */
    bsp_shell_init();

    //multiTimerStart(&s_timer_log, 500, log_test_callback, NULL);

    /* 启动串口测试验证模块 */
    //app_uart_demo_init();
    app_shell_demo_init();
    app_rgb_demo_init();

    /* 后续在此注册其他常驻任务 */
}

/**
 * @brief 应用层主轮询任务处理
 */
void app_main_process(void)
{
    multiTimerYield();
    bsp_shell_process();
}
