/**
 * @file app_lvgl_fs_demo.c
 * @brief LVGL 文件系统集成自检测试演示模块实现
 */

#define LOG_TAG "APP_LVGL_FS"
#include "app_lvgl_fs_demo.h"
#include "lvgl.h"
#include "bsp_logger.h"
#include "shell.h"
#include <string.h>

static void run_lvgl_fs_test(void)
{
    log_i("Starting LVGL File System test on 'F' drive...");

    lv_fs_file_t f;
    lv_fs_res_t res = lv_fs_open(&f, "F:/test_lvgl_fs.txt", LV_FS_MODE_WR);
    if (res == LV_FS_RES_OK)
    {
        const char *write_str = "LVGL File System integrated successfully with LittleFS!";
        uint32_t btw = strlen(write_str);
        uint32_t bw = 0;
        lv_fs_write(&f, write_str, btw, &bw);
        lv_fs_close(&f);
        log_i("LVGL FS: Write success, bytes written = %lu", (unsigned long)bw);

        /* 再次读取验证 */
        res = lv_fs_open(&f, "F:/test_lvgl_fs.txt", LV_FS_MODE_RD);
        if (res == LV_FS_RES_OK)
        {
            char read_buf[64] = {0};
            uint32_t br = 0;
            lv_fs_read(&f, read_buf, sizeof(read_buf) - 1, &br);
            lv_fs_close(&f);
            log_i("LVGL FS: Read data = [%s], bytes read = %lu", read_buf, (unsigned long)br);
        }
        else
        {
            log_e("LVGL FS: Read failed! res = %d", res);
        }
    }
    else
    {
        log_e("LVGL FS: Write open failed! res = %d", res);
    }
}

/**
 * @brief 初始化 LVGL 文件系统演示模块
 */
bsp_status_t app_lvgl_fs_demo_init(void)
{
    /* 执行一次自检测试 */
    run_lvgl_fs_test();
    return BSP_OK;
}

/**
 * @brief Shell 指令：触发 LVGL 文件系统读写测试
 */
static void shell_lvgl_fs_test(void)
{
    run_lvgl_fs_test();
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), 
                 lvgl_fs_test, shell_lvgl_fs_test, "Test LVGL file system read and write on LittleFS");
