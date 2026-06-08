/**
 * @file app_sys_monitor.c
 * @brief 系统状态监视器任务实现
 */

#include "app_sys_monitor.h"
#include "MultiTimer.h"
#include "bsp_shell.h"
#include "bsp_logger.h"
#include "dev_key.h"
#include "dev_led.h"
#include "dev_buzzer.h"
#include "dev_ws2812.h"

/* --- 静态变量定义 --- */

static sys_state_t g_current_state = SYS_STATE_IDLE;
static sys_event_t g_pending_event = SYS_EVENT_NONE;
static bool        g_is_muted      = false;

/* 定时器定义 */
static MultiTimer timer_fsm;       /* 状态机驱动定时器 */
static MultiTimer timer_heartbeat; /* 核心板 LED 心跳定时器 */
static MultiTimer timer_rgb;       /* WS2812 刷新定时器 */
static MultiTimer timer_buzzer;    /* 蜂鸣器滴答定时器 */
static MultiTimer timer_key;       /* 按键扫描定时器 */

/* --- 静态函数声明 --- */

static void key2_short_cb(Button *btn);
static void key2_long_cb(Button *btn);
static void key2_double_cb(Button *btn);

static void fsm_timer_cb(MultiTimer *timer, void *arg);
static void heartbeat_timer_cb(MultiTimer *timer, void *arg);
static void rgb_timer_cb(MultiTimer *timer, void *arg);
static void buzzer_timer_cb(MultiTimer *timer, void *arg);
static void key_timer_cb(MultiTimer *timer, void *arg);

static void switch_state(sys_state_t new_state);

/* --- 按键回调实现 (仅发送事件，不控制外设) --- */

static void key2_short_cb(Button *btn)
{
    // 向状态机发送 SYS_EVENT_NEXT 事件
    app_sys_monitor_post_event(SYS_EVENT_NEXT);
}

static void key2_long_cb(Button *btn)
{
    // 向状态机发送 SYS_EVENT_ALARM 事件
    app_sys_monitor_post_event(SYS_EVENT_ALARM);
}

static void key2_double_cb(Button *btn)
{
    // 向状态机发送 SYS_EVENT_MUTE_TOGGLE 事件
    app_sys_monitor_post_event(SYS_EVENT_MUTE_TOGGLE);
}

/* --- 状态机流转与外设控制逻辑 --- */

void app_sys_monitor_post_event(sys_event_t event)
{
    if (event >= SYS_EVENT_MAX) return;
    g_pending_event = event;
}

sys_state_t app_sys_monitor_get_state(void)
{
    return g_current_state;
}

static void switch_state(sys_state_t new_state)
{
    sys_state_t old_state = g_current_state;

    if (old_state == new_state)
        return;

    g_current_state = new_state;
    log_i("[FSM] State changed: %d -> %d", old_state, new_state);

    if (!g_is_muted)
    {
        dev_buzzer_on();
        multiTimerStart(&timer_buzzer, 50, buzzer_timer_cb, NULL);
    }
}

static void fsm_timer_cb(MultiTimer *timer, void *arg)
{
    sys_event_t event = g_pending_event;
    g_pending_event   = SYS_EVENT_NONE;

    if (event != SYS_EVENT_NONE)
    {
        if (event == SYS_EVENT_MUTE_TOGGLE)
        {
            g_is_muted = !g_is_muted;
            log_i("[FSM] Mute toggled: %s", g_is_muted ? "ON" : "OFF");
        }
        else
        {
            switch (g_current_state)
            {
            case SYS_STATE_IDLE:
                if (event == SYS_EVENT_NEXT)
                    switch_state(SYS_STATE_WORK);
                else if (event == SYS_EVENT_ALARM)
                    switch_state(SYS_STATE_ALARM);
                break;
            case SYS_STATE_WORK:
                if (event == SYS_EVENT_NEXT)
                    switch_state(SYS_STATE_IDLE);
                else if (event == SYS_EVENT_ALARM)
                    switch_state(SYS_STATE_ALARM);
                break;
            case SYS_STATE_ALARM:
                /* 长按（EVENT_ALARM）代表解除报警，退回 IDLE */
                if (event == SYS_EVENT_ALARM)
                    switch_state(SYS_STATE_IDLE);
                break;
            default:
                break;
            }
        }
    }

    // 无论是否有事件，都续期定时器，保持状态机持续运行
    multiTimerStart(timer, 10, fsm_timer_cb, arg);
}

static void heartbeat_timer_cb(MultiTimer *timer, void *arg)
{
    static uint32_t ticks = 0;
    ticks++;
    if (g_current_state == SYS_STATE_IDLE)
    {
        if (ticks % 10 == 0) dev_led_toggle(LED_CORE);
    }
    else if (g_current_state == SYS_STATE_WORK)
    {
        dev_led_set(LED_CORE, DEV_LED_ON);
    }
    else if (g_current_state == SYS_STATE_ALARM)
    {
        dev_led_toggle(LED_CORE); // 配合50ms的timer可以实现极速闪烁
    }
    multiTimerStart(timer, 50, heartbeat_timer_cb, arg);
}

/**
 * @brief 简单的 HSV 到 RGB 转换函数，用于生成彩虹效果
 */
static dev_ws2812_rgb_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v)
{
    dev_ws2812_rgb_t rgb = {0, 0, 0};
    uint8_t region, remainder, p, q, t;

    if (s == 0)
    {
        rgb.r = v; rgb.g = v; rgb.b = v;
        return rgb;
    }

    region = (h / 43) % 6;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0: rgb.r = v; rgb.g = t; rgb.b = p; break;
        case 1: rgb.r = q; rgb.g = v; rgb.b = p; break;
        case 2: rgb.r = p; rgb.g = v; rgb.b = t; break;
        case 3: rgb.r = p; rgb.g = q; rgb.b = v; break;
        case 4: rgb.r = t; rgb.g = p; rgb.b = v; break;
        default:rgb.r = v; rgb.g = p; rgb.b = q; break;
    }
    return rgb;
}

static void rgb_timer_cb(MultiTimer *timer, void *arg)
{
    static uint16_t loop = 0;
    loop++;

    if (g_current_state == SYS_STATE_IDLE)
    {
        /* 待机：紫色呼吸 */
        uint8_t brightness = (loop / 2) % 100;
        if (brightness > 50) brightness = 100 - brightness;
        dev_ws2812_rgb_t color = {brightness * 2, 0, brightness * 2};
        dev_ws2812_set_all(color);
    }
    else if (g_current_state == SYS_STATE_WORK)
    {
        /* 工作：彩虹跑马 */
        for (uint16_t i = 0; i < DEV_WS2812_LED_COUNT; i++)
        {
            uint16_t h = (loop * 5 + i * 85) % 256;
            dev_ws2812_rgb_t color = hsv_to_rgb(h, 255, 50);
            dev_ws2812_set_color(i, color);
        }
    }
    else if (g_current_state == SYS_STATE_ALARM)
    {
        /* 报警：红色急闪 */
        if (loop % 4 < 2)
        {
            dev_ws2812_set_all((dev_ws2812_rgb_t){255, 0, 0});
        }
        else
        {
            dev_ws2812_set_all(DEV_WS2812_COLOR_BLACK);
        }
    }
    
    dev_ws2812_refresh();
    multiTimerStart(timer, 20, rgb_timer_cb, arg);
}

static void buzzer_timer_cb(MultiTimer *timer, void *arg)
{
    dev_buzzer_off();
}

static void key_timer_cb(MultiTimer *timer, void *arg)
{
    dev_key_scan();
    multiTimerStart(timer, 5, key_timer_cb, arg);
}

/* --- 初始化入口 --- */

bsp_status_t app_sys_monitor_init(void)
{
    log_i("Initializing System Status Monitor Task...");

    /* 1. 初始化底层硬件 (LED, Buzzer, Key, WS2812) */
    dev_led_init();
    dev_buzzer_init();
    dev_key_init();
    dev_ws2812_init();

    /* 2. 注册按键回调 */
    dev_key_attach(DEV_KEY2, BTN_SINGLE_CLICK, key2_short_cb);
    dev_key_attach(DEV_KEY2, BTN_LONG_PRESS_START, key2_long_cb);
    dev_key_attach(DEV_KEY2, BTN_DOUBLE_CLICK, key2_double_cb);

    /* 3. 启动定时器 */
    multiTimerStart(&timer_fsm, 10, fsm_timer_cb, NULL);
    multiTimerStart(&timer_heartbeat, 50, heartbeat_timer_cb, NULL);
    multiTimerStart(&timer_rgb, 20, rgb_timer_cb, NULL);
    multiTimerStart(&timer_key, 5, key_timer_cb, NULL);

    /* 4. 初始状态设定 */
    // switch_state(SYS_STATE_IDLE);
    g_pending_event = SYS_EVENT_NEXT;

    return BSP_OK;
}

/* --- Shell 命令扩展 --- */

static void sys_set_state(int state)
{
    if (state == 0)
    {
        switch_state(SYS_STATE_IDLE);
    }
    else if (state == 1)
    {
        switch_state(SYS_STATE_WORK);
    }
    else if (state == 2)
    {
        switch_state(SYS_STATE_ALARM);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN,
                 sys_set_state, sys_set_state, "Force system state (0:IDLE, 1:WORK, 2:ALARM)");
