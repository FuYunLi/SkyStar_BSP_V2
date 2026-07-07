/**
 * @file app_lvgl_images_demo.c
 * @brief LVGL 编码器控制相册展示自检演示模块实现
 */

#define LOG_TAG "APP_ALBUM"

#include "app_lvgl_images_demo.h"
#include "bsp_lfs.h"
#include "bsp_ec11.h"
#include "dev_key.h"
#include "dev_led.h"
#include "bsp_logger.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

#define MAX_ALBUM_IMAGES    32
#define MAX_IMAGE_PATH_LEN  80
#define IMAGE_DIR           "/LVGL/asset/Naraka"

/* 界面色彩配置 (深色 premium 风格) */
#define COLOR_BG            0x0B0F19  /* 极深午夜蓝背景 */
#define COLOR_CARD_BG       0x151B2D  /* 暗海军蓝卡片背景 */
#define COLOR_CARD_BORDER   0x202D4A  /* 柔和科技蓝边框 */
#define COLOR_TEXT_TITLE    0x38BDF8  /* 亮天蓝色标题 */
#define COLOR_TEXT_FILE     0xFFFFFF  /* 纯白色文件名 */
#define COLOR_TEXT_INDEX    0x34D399  /* 翡翠绿索引指示器 */

/* ================================================================
 * 结构体定义
 * ================================================================ */

typedef struct
{
    char     paths[MAX_ALBUM_IMAGES][MAX_IMAGE_PATH_LEN];
    char     names[MAX_ALBUM_IMAGES][64];
    uint32_t sizes[MAX_ALBUM_IMAGES];
    uint8_t  count;
    int16_t  current_index;
} app_album_t;

/* ================================================================
 * 私有静态变量
 * ================================================================ */

static app_album_t s_album = {0};

/* LVGL UI 控件指针 */
static lv_obj_t *s_card = NULL;
static lv_obj_t *s_img = NULL;
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_filename_label = NULL;
static lv_obj_t *s_index_label = NULL;

/* ================================================================
 * 私有静态函数
 * ================================================================ */

static void debug_dump_image_header(const char *path)
{
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t file;
    const char *lfs_path = path;
    if (path[0] == 'F' && path[1] == ':')
    {
        lfs_path = path + 2;
    }

    int err = lfs_file_open(lfs, &file, lfs_path, LFS_O_RDONLY);
    if (err < 0)
    {
        log_e("Debug: Failed to open %s in lfs (err: %d)", lfs_path, err);
        return;
    }

    uint8_t buf[32] = {0};
    lfs_ssize_t read_len = lfs_file_read(lfs, &file, buf, sizeof(buf));
    lfs_file_close(lfs, &file);

    if (read_len < 0)
    {
        log_e("Debug: Failed to read %s (err: %d)", lfs_path, (int)read_len);
        return;
    }

    log_i("Debug dump of %s (read %d bytes):", lfs_path, (int)read_len);
    for (int i = 0; i < read_len; i += 16)
    {
        char hex_str[64] = {0};
        char *ptr = hex_str;
        for (int j = 0; j < 16 && (i + j) < read_len; j++)
        {
            ptr += snprintf(ptr, 4, "%02X ", buf[i + j]);
        }
        log_i("  %s", hex_str);
    }
}

/**
 * @brief 检索 LittleFS 中的图像文件
 */
static void scan_images(void)
{
    /* 确保 LittleFS 已成功挂载 */
    if (bsp_lfs_mount() != BSP_OK)
    {
        log_e("Failed to mount LittleFS for image scanning.");
        return;
    }

    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_dir_t dir;
    struct lfs_info info;

    int err = lfs_dir_open(lfs, &dir, IMAGE_DIR);
    if (err != LFS_ERR_OK)
    {
        log_w("Directory %s not found (err: %d)", IMAGE_DIR, err);
        return;
    }

    s_album.count = 0;
    while (lfs_dir_read(lfs, &dir, &info) > 0 && s_album.count < MAX_ALBUM_IMAGES)
    {
        if (info.type == LFS_TYPE_REG)
        {
            size_t len = strlen(info.name);
            if (len > 4 && strcmp(info.name + len - 4, ".bin") == 0)
            {
                snprintf(s_album.paths[s_album.count], MAX_IMAGE_PATH_LEN, "F:%s/%s", IMAGE_DIR, info.name);
                snprintf(s_album.names[s_album.count], sizeof(s_album.names[0]), "%s", info.name);
                s_album.sizes[s_album.count] = info.size;
                s_album.count++;
            }
        }
    }

    lfs_dir_close(lfs, &dir);

    log_i("Scanned %d image files in %s:", s_album.count, IMAGE_DIR);
    for (uint8_t i = 0; i < s_album.count; i++)
    {
        log_i("  [%d] %s (size: %lu bytes, path: %s)", i, s_album.names[i], (unsigned long)s_album.sizes[i], s_album.paths[i]);
        if (i < 3) // Dump headers for first 3 files
        {
            debug_dump_image_header(s_album.paths[i]);
        }
    }
}


/**
 * @brief KEY1 短按触发回调，打印当前图片元数据并翻转核心板 LED
 */
static void key1_click_cb(Button *btn)
{
    (void)btn;
    dev_led_toggle(LED_CORE);

    if (s_album.count > 0 && s_album.current_index >= 0 && s_album.current_index < s_album.count)
    {
        log_i("KEY1 pressed! Current photo metadata:");
        log_i("  Index: %d / %d", s_album.current_index + 1, s_album.count);
        log_i("  Name:  %s", s_album.names[s_album.current_index]);
        log_i("  Path:  %s", s_album.paths[s_album.current_index]);
        log_i("  Size:  %lu bytes", (unsigned long)s_album.sizes[s_album.current_index]);
    }
    else
    {
        log_w("KEY1 pressed but album is empty or index invalid.");
    }
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 LVGL 相册展示演示模块，检索 LittleFS 图像并初始化 UI
 */
bsp_status_t app_lvgl_images_demo_init(void)
{
    log_i("Initializing LVGL Images Album Demo...");

    /* 1. 扫描 LittleFS 中的 .bin 图像文件 */
    scan_images();

    /* 2. 获取活动屏幕并配置深色底色 */
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 3. 如果未检索到图片，显示友好提示 */
    if (s_album.count == 0)
    {
        s_filename_label = lv_label_create(scr);
        lv_label_set_text(s_filename_label, "No images found in\n/LVGL/asset/Naraka\nPlease upload via Ymodem!");
        lv_obj_set_style_text_color(s_filename_label, lv_color_hex(0xEF4444), 0); /* 亮红色警告 */
        lv_obj_set_style_text_align(s_filename_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(s_filename_label, LV_ALIGN_CENTER, 0, 0);

        log_w("No images found, initialized interface with error message.");
        return BSP_OK;
    }

    /* 4. 创建 UI 组件 */
    s_album.current_index = 0;

    /* 标题 Label */
    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, "NARAKA GALLERY");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(COLOR_TEXT_TITLE), 0);
    lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 15);

    /* 卡片式边框容器 */
    s_card = lv_obj_create(scr);
    lv_obj_set_size(s_card, 212, 212);
    lv_obj_align(s_card, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(s_card, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_color(s_card, lv_color_hex(COLOR_CARD_BORDER), 0);
    lv_obj_set_style_border_width(s_card, 2, 0);
    lv_obj_set_style_radius(s_card, 12, 0);
    lv_obj_set_style_pad_all(s_card, 4, 0);
    lv_obj_remove_flag(s_card, LV_OBJ_FLAG_SCROLLABLE);

    /* 图像控件 */
    s_img = lv_image_create(s_card);
    lv_obj_set_size(s_img, 200, 200);
    lv_obj_align(s_img, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_src(s_img, s_album.paths[0]);

    /* 文件名 Label */
    s_filename_label = lv_label_create(scr);
    lv_label_set_text(s_filename_label, s_album.names[0]);
    lv_obj_set_style_text_color(s_filename_label, lv_color_hex(COLOR_TEXT_FILE), 0);
    lv_obj_set_style_text_align(s_filename_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_filename_label, LV_ALIGN_BOTTOM_MID, 0, -40);

    /* 索引页码 Label */
    s_index_label = lv_label_create(scr);
    char buf[32];
    snprintf(buf, sizeof(buf), "[ 1 / %d ]", s_album.count);
    lv_label_set_text(s_index_label, buf);
    lv_obj_set_style_text_color(s_index_label, lv_color_hex(COLOR_TEXT_INDEX), 0);
    lv_obj_set_style_text_align(s_index_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_index_label, LV_ALIGN_BOTTOM_MID, 0, -15);

    /* 5. 绑定 KEY1 交互回调 */
    dev_key_attach(DEV_KEY1, BTN_SINGLE_CLICK, key1_click_cb);

    log_i("LVGL Images Album Demo UI components created successfully.");
    return BSP_OK;
}

/**
 * @brief LVGL 相册周期轮询处理函数，负责检测 EC11 编码器与按键交互
 */
void app_lvgl_images_demo_process(void)
{
    static uint32_t last_poll_time = 0;
    uint32_t now = bsp_tick_get_ms();

    /* 限制轮询频率为 20ms */
    if (now - last_poll_time < 20)
    {
        return;
    }
    last_poll_time = now;

    if (s_album.count == 0)
    {
        return;
    }

    bsp_ec11_info_t ec_info = {0};
    bsp_status_t status = bsp_ec11_get_info(&ec_info);
    if (status == BSP_OK && ec_info.dir != 0)
    {
        int16_t next_idx = s_album.current_index;
        if (ec_info.dir > 0)
        {
            /* 顺时针旋转，显示下一张 */
            next_idx++;
            if (next_idx >= s_album.count)
            {
                next_idx = 0;
            }
        }
        else if (ec_info.dir < 0)
        {
            /* 逆时针旋转，显示上一张 */
            next_idx--;
            if (next_idx < 0)
            {
                next_idx = s_album.count - 1;
            }
        }

        if (next_idx != s_album.current_index)
        {
            s_album.current_index = next_idx;
            lv_image_set_src(s_img, s_album.paths[s_album.current_index]);
            lv_label_set_text(s_filename_label, s_album.names[s_album.current_index]);

            char buf[32];
            snprintf(buf, sizeof(buf), "[ %d / %d ]", s_album.current_index + 1, s_album.count);
            lv_label_set_text(s_index_label, buf);

            log_i("[Album] Selected: [%d] %s", s_album.current_index + 1, s_album.names[s_album.current_index]);
        }
    }
}
