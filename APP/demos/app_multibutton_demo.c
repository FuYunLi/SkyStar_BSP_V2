#include "app_multibutton_demo.h"
#include "dev_key.h"
#include "MultiTimer.h"
#include "shell.h"
#include "dev_led.h"
#include <stdint.h>
#define LOG_TAG "BUTTON_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>

/* 按键扫描定时器回调 */
static MultiTimer timer_key;
static MultiTimer timer_led_core;

static void key_timer_cb(MultiTimer *timer, void *userdata)
{
    dev_key_scan();
    multiTimerStart(timer, 10, key_timer_cb, userdata);
}

static void led_timer_cb(MultiTimer *timer, void *userdata)
{
    dev_led_toggle(LED_CORE);
    multiTimerStart(timer, 500, led_timer_cb, userdata);
}

/* 不再需要在这里定义 Button 变量，因为 dev_key.c 已经在底层封装了 */

static void key1_short_cb(Button *btn, void *userdata)
{
    log_d("Key1 short press");
    dev_led_toggle(LED_CORE);
}

static void key1_long_cb(Button *btn, void *userdata)
{
    log_d("Key1 long press");
    multiTimerStart(&timer_led_core, 500, led_timer_cb, NULL);
}

static void key1_double_cb(Button *btn, void *userdata)
{
    log_d("Key1 double press");
    multiTimerStop(&timer_led_core);
    dev_led_set(LED_CORE, DEV_LED_OFF);
}

bsp_status_t app_multibutton_demo_init(void)
{
    /* 底层按键硬件与中间件初始化 */
    dev_key_init();
    
    /* 注册事件回调 */
    dev_key_attach(DEV_KEY1, BTN_SINGLE_CLICK, key1_short_cb);
    dev_key_attach(DEV_KEY1, BTN_LONG_PRESS_START, key1_long_cb);
    dev_key_attach(DEV_KEY1, BTN_DOUBLE_CLICK, key1_double_cb);

    /* 启动按键扫描定时器 (10ms) */
    multiTimerStart(&timer_key, 10, key_timer_cb, NULL);
    
    return BSP_OK;
}

void app_key_demo(void)
{
    log_i("Starting MultiButton Demo...");
    app_multibutton_demo_init();
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, key, app_key_demo, "key demo");

