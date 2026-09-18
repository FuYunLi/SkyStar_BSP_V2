/**
 * @file app_mic_demo.c
 * @brief 麦克风录音自检演示实现
 * @note 对标官方出厂 drv_mic。录音由 bsp_mic 双缓冲机制承担。
 *       【未验证】录音链路尚未上板实测。
 */

#define LOG_TAG "APP_MIC"

#include "app_mic_demo.h"
#include "bsp_logger.h"
#include "bsp_mic.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief mic_record Shell 指令入口：录音到 SD 卡
 */
static int shell_mic_record(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_i("usage: mic_record <path> [seconds] [sample_rate]");
        return -1;
    }

    uint32_t seconds = (argc >= 3) ? (uint32_t)atoi(argv[2]) : 5U;
    uint32_t rate = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 16000U;

    bsp_status_t ret = bsp_mic_record(argv[1], seconds, rate);
    if (ret == BSP_BUSY)
    {
        log_w("recording already running.");
        return -1;
    }
    if (ret != BSP_OK)
    {
        log_e("record start failed! ret = %d", ret);
        return -1;
    }

    return 0;
}

/**
 * @brief mic_stop Shell 指令入口：提前停止录音
 */
static int shell_mic_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bsp_status_t ret = bsp_mic_stop();
    if (ret != BSP_OK)
    {
        return -1;
    }

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化麦克风录音演示模块
 */
bsp_status_t app_mic_demo_init(void)
{
    log_i("Mic Demo loaded. Try: mic_record 0:/rec/test.wav 5 16000");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, mic_record, shell_mic_record, Record mic to WAV on SD card);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, mic_stop, shell_mic_stop, Stop recording early);
