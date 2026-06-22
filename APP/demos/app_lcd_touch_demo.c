/**
 * @file app_lcd_touch_demo.c
 * @brief 屏幕划线与触摸反馈综合测试 Demo 实现
 * @note 基于 LVGL 自定义控件 + lv_draw_line 绘图，天然融入 LVGL 渲染管线，
 *       无资源冲突，零额外帧缓冲区开销。
 *       支持按键清屏与 Shell 命令清屏。
 *       手指滑动时在屏幕上实时绘制平滑触摸轨迹。
 */

#include "app_lcd_touch_demo.h"
#include "dev_key.h"
#include "dev_ft6336.h"
#include "port_tick.h"
#include "shell.h"

#include "lvgl.h"
#include "examples/porting/lv_port_indev.h"

/* 默认触摸板输入设备（定义在 lv_port_indev.c） */
extern lv_indev_t *indev_touchpad;

#define LOG_TAG "LCD_TOUCH"
#include "bsp_logger.h"

/* ================================================================
 * 画线 Demo 常量定义
 * ================================================================ */

/* 画笔线宽（像素） */
#define DRAW_LINE_WIDTH 3

/* 触摸点 1 画笔颜色（青色） */
#define DRAW_COLOR_P1_VAL 0x00FFFF

/* 触摸点 2 画笔颜色（粉色） */
#define DRAW_COLOR_P2_VAL 0xFF80FF

/* 画布背景色（黑色） */
#define DRAW_BG_COLOR_VAL 0x000000

/* 按键清屏触发键 */
#define DRAW_CLEAR_KEY DEV_KEY1

/* 最大存储线段数量 */
#define DRAW_MAX_LINES 512

/* ================================================================
 * 线段存储结构
 * ================================================================ */

typedef struct
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    uint8_t color_id; /* 0=P1, 1=P2 */
} draw_line_t;

/* ================================================================
 * 模块内部状态
 * ================================================================ */

/** 画线模式运行标志 */
static volatile bool s_draw_running = false;

/** LVGL 画板对象指针 */
static lv_obj_t *s_draw_obj = NULL;

/** 线段存储数组 */
static draw_line_t s_lines[DRAW_MAX_LINES];

/** 当前线段数量 */
static uint16_t s_line_count = 0;

/** 各触摸点上一次有效坐标 */
typedef struct
{
    bool    valid;
    int16_t x;
    int16_t y;
} draw_last_point_t;

static draw_last_point_t s_last_points[2];

/** 画线模式专用的 LVGL 输入设备 */
static lv_indev_t *s_draw_indev = NULL;

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * @brief 添加一条线段到存储数组
 * @retval true 添加成功
 * @retval false 数组已满
 */
static bool draw_add_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color_id)
{
    if (s_line_count >= DRAW_MAX_LINES)
    {
        return false;
    }

    s_lines[s_line_count].x0       = x0;
    s_lines[s_line_count].y0       = y0;
    s_lines[s_line_count].x1       = x1;
    s_lines[s_line_count].y1       = y1;
    s_lines[s_line_count].color_id = color_id;
    s_line_count++;

    return true;
}

/**
 * @brief 清空所有线段并重绘
 */
static void draw_clear_all(void)
{
    s_line_count = 0;

    for (int i = 0; i < 2; i++)
    {
        s_last_points[i].valid = false;
    }

    /* 标记对象为脏区域，触发重绘 */
    if (s_draw_obj != NULL)
    {
        lv_obj_invalidate(s_draw_obj);
    }
}

/**
 * @brief KEY1 按键按下回调：清屏
 */
static void draw_key_clear_cb(void *btn)
{
    (void)btn;
    if (!s_draw_running)
    {
        return;
    }

    log_i("KEY1 pressed, clearing canvas...");
    draw_clear_all();
}

/* ================================================================
 * LVGL 自定义画板控件的绘制回调
 * ================================================================ */

/**
 * @brief 画板控件的事件回调
 * @note LV_EVENT_DRAW_MAIN 事件中用 lv_draw_line 绘制所有存储的线段，
 *       天然融入 LVGL 渲染管线，无需额外帧缓冲区。
 *       事件参数为 lv_layer_t *，可直接用于 lv_draw_line。
 */
static void draw_obj_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_MAIN)
    {
        return;
    }

    lv_layer_t *layer = lv_event_get_param(e);

    /* 绘制所有存储的线段 */
    for (uint16_t i = 0; i < s_line_count; i++)
    {
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);

        line_dsc.p1.x        = s_lines[i].x0;
        line_dsc.p1.y        = s_lines[i].y0;
        line_dsc.p2.x        = s_lines[i].x1;
        line_dsc.p2.y        = s_lines[i].y1;
        line_dsc.width       = DRAW_LINE_WIDTH;
        line_dsc.round_start = 1;
        line_dsc.round_end   = 1;
        line_dsc.opa         = LV_OPA_COVER;

        if (s_lines[i].color_id == 0)
        {
            line_dsc.color = lv_color_hex(DRAW_COLOR_P1_VAL);
        }
        else
        {
            line_dsc.color = lv_color_hex(DRAW_COLOR_P2_VAL);
        }

        lv_draw_line(layer, &line_dsc);
    }
}

/* ================================================================
 * LVGL 输入设备读取回调
 * ================================================================ */

/**
 * @brief 画线模式输入设备读取回调：读取触摸硬件并绘制轨迹
 * @note 在 LVGL 的 lv_timer_handler() 中被自动调用，
 *       天然与 LVGL 渲染管线同步，无资源冲突。
 *       回调职责：1) 从 FT6336 读取触摸坐标填入 data
 *                2) 按下时将线段存入数组并标记重绘
 */
static void draw_indev_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    /* 从触摸芯片读取最新数据 */
    bsp_status_t status = dev_ft6336_read_touch(&touch_dev);

    if (status == BSP_OK && touch_dev.touch_count > 0 && touch_dev.points[0].valid)
    {
        data->state   = LV_INDEV_STATE_PRESSED;
        data->point.x = (int16_t)touch_dev.points[0].x;
        data->point.y = (int16_t)touch_dev.points[0].y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    if (!s_draw_running || s_draw_obj == NULL)
    {
        return;
    }

    if (data->state == LV_INDEV_STATE_PRESSED)
    {
        int x = data->point.x;
        int y = data->point.y;

        /* 坐标越界保护 */
        if (x < 0 || x >= 240 || y < 0 || y >= 320)
        {
            return;
        }

        if (s_last_points[0].valid)
        {
            /* 有上一点，添加连线 */
            if (draw_add_line(s_last_points[0].x, s_last_points[0].y, x, y, 0))
            {
                lv_obj_invalidate(s_draw_obj);
            }
        }
        else
        {
            /* 按下首点，画一个极短线段（退化为点） */
            if (draw_add_line(x, y, x, y, 0))
            {
                lv_obj_invalidate(s_draw_obj);
            }
        }

        s_last_points[0].valid = true;
        s_last_points[0].x     = (int16_t)x;
        s_last_points[0].y     = (int16_t)y;
    }
    else
    {
        /* 手指抬起，清除历史坐标 */
        s_last_points[0].valid = false;
    }
}

/* ================================================================
 * Shell 命令实现
 * ================================================================ */

/**
 * @brief lcd_draw 命令：启动/停止画线模式
 * @note 无参数启动画线，传入 stop 停止画线
 */
int shell_lcd_draw(int argc, char *argv[])
{
    if (argc >= 2 && (strcmp(argv[1], "stop") == 0 || strcmp(argv[1], "0") == 0))
    {
        if (!s_draw_running)
        {
            log_w("Drawing mode is not running.");
            return 0;
        }

        s_draw_running = false;

        /* 删除画线输入设备 */
        if (s_draw_indev != NULL)
        {
            lv_indev_delete(s_draw_indev);
            s_draw_indev = NULL;
        }

        /* 删除画板对象，恢复原始 LVGL 界面 */
        if (s_draw_obj != NULL)
        {
            lv_obj_delete(s_draw_obj);
            s_draw_obj = NULL;
        }

        /* 恢复默认触摸板 */
        if (indev_touchpad != NULL)
        {
            lv_indev_enable(indev_touchpad, true);
        }

        s_line_count = 0;

        log_i("Drawing mode stopped. LVGL UI restored.");
        return 0;
    }

    if (s_draw_running)
    {
        log_w("Drawing mode is already running. Use 'lcd_draw stop' to stop.");
        return 0;
    }

    /* 创建全屏画板对象 */
    lv_obj_t *scr = lv_screen_active();
    s_draw_obj    = lv_obj_create(scr);
    lv_obj_set_size(s_draw_obj, 240, 320);
    lv_obj_center(s_draw_obj);
    lv_obj_set_style_bg_color(s_draw_obj, lv_color_hex(DRAW_BG_COLOR_VAL), 0);
    lv_obj_set_style_border_width(s_draw_obj, 0, 0);
    lv_obj_clear_flag(s_draw_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_draw_obj, LV_OBJ_FLAG_CLICKABLE);

    /* 注册绘制事件回调 */
    lv_obj_add_event_cb(s_draw_obj, draw_obj_event_cb, LV_EVENT_DRAW_MAIN, NULL);

    /* 清空线段存储 */
    s_line_count = 0;
    for (int i = 0; i < 2; i++)
    {
        s_last_points[i].valid = false;
    }

    /* 注册 KEY1 按键清屏回调 */
    dev_key_attach(DRAW_CLEAR_KEY, BTN_PRESS_DOWN, draw_key_clear_cb);

    /* 禁用默认触摸板，避免两个输入设备同时处理触摸事件 */
    if (indev_touchpad != NULL)
    {
        lv_indev_enable(indev_touchpad, false);
    }

    /* 创建画线模式专用的输入设备 */
    s_draw_indev = lv_indev_create();
    lv_indev_set_type(s_draw_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_draw_indev, draw_indev_read_cb);

    s_draw_running = true;

    log_i("Drawing mode started. Touch the screen to draw.");
    log_i("Press KEY1 or use 'lcd_clear' to clear canvas.");
    log_i("Use 'lcd_draw stop' to exit and restore LVGL UI.");

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, lcd_draw,
                 shell_lcd_draw, "Start/stop touch drawing. Usage: lcd_draw [stop]");

/**
 * @brief lcd_clear 命令：清空当前画布
 */
int shell_lcd_clear(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    draw_clear_all();
    log_i("Canvas cleared.");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, lcd_clear,
                 shell_lcd_clear, "Clear the drawing canvas to black");

/* ================================================================
 * 公开接口 API
 * ================================================================ */

/**
 * @brief 初始化屏幕划线与触摸反馈 Demo
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_lcd_touch_demo_init(void)
{
    log_i("[APP] LCD touch drawing demo initialized");
    return BSP_OK;
}

/**
 * @brief 查询画线模式是否正在运行
 * @retval true 画线模式激活中
 * @retval false 画线模式未激活
 */
bool app_lcd_touch_demo_is_running(void)
{
    return s_draw_running;
}
