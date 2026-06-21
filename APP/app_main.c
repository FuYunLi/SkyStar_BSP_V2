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
#include "dev_key.h"
#include "dev_led.h"
#include "bsp_led.h"
#include "dev_buzzer.h"
#include "dev_ws2812.h"
#include "bsp_shell.h"
#include "bsp_storage.h"

#define LOG_TAG "APP_MAIN"
#include "bsp_logger.h"
#include "demos/app_uart_demo.h"
#include "demos/app_shell_demo.h"
#include "demos/app_rgb_demo.h"
#include "demos/app_storage_demo.h"
#include "demos/app_spi_demo.h"
#include "demos/app_flash_demo.h"
#include "demos/app_lcd_demo.h"
#include "demos/app_touch_demo.h"
#include "demos/app_lvgl_fs_demo.h"
#include "port_spi.h"
#include "dev_st7789.h"
#include "tasks/app_sys_monitor.h"

/* LVGL 头文件 */
#include "lvgl.h"
#include "examples/porting/lv_port_disp.h"
#include "examples/porting/lv_port_indev.h"
#include "examples/porting/lv_port_fs.h"

/* LVGL 测试按钮回调 */
static void btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn   = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_obj_get_user_data(btn);

    static bool clicked = false;
    clicked             = !clicked;

    if (clicked)
    {
        lv_label_set_text(label, "Clicked!");
    }
    else
    {
        lv_label_set_text(label, "Not Clicked");
    }
}

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
    bsp_led_init();
    dev_buzzer_init();
    dev_ws2812_init();

    /* 初始化 SPI 通道 */
    port_spi_init(PORT_SPI_1);
    port_spi_init(PORT_SPI_2);

    /* 初始化 LCD 物理驱动及背光 */
    lcd_init();

    /* 初始化日志服务并打印测试日志 */
    bsp_logger_init();
    (void)bsp_storage_init();
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

    /* LVGL 初始化 */
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_port_fs_init();

    /* 创建极简测试界面：白色背景 + 蓝色按钮 + 状态提示 */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);

    /* 创建蓝色按钮 */
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x0000FF), 0);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    /* 按钮上的文字 */
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Button");
    lv_obj_center(btn_label);

    /* 状态提示标签 */
    lv_obj_t *status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Not Clicked");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 40);

    /* 将状态标签保存到按钮的user_data */
    lv_obj_set_user_data(btn, status_label);

    //multiTimerStart(&s_timer_log, 500, log_test_callback, NULL);

    /* 启动串口测试验证模块 */
    //app_uart_demo_init();
    app_shell_demo_init();
    app_rgb_demo_init();
    app_storage_demo_init();
    app_spi_demo_init();
    app_flash_demo_init();
    app_lcd_demo_init();
    app_touch_demo_init();
    app_sys_monitor_init();
    app_lvgl_fs_demo_init();

    /* 后续在此注册其他常驻任务 */
}

/**
 * @brief 应用层主轮询任务处理
 */
void app_main_process(void)
{
    multiTimerYield();
    bsp_shell_process();

    /* LVGL GUI 任务处理 */
    (void)lv_timer_handler();
}
