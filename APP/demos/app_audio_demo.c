/**
 * @file app_audio_demo.c
 * @brief SD 卡 WAV 播放自检演示实现
 * @note 对标 RocketPi sd_audio_to_i2s 简化版。播放调度由 bsp_audio
 *       双缓冲机制承担，本模块仅导出 Shell 控制指令。
 *       硬件前置条件：SD 卡插入、FatFS 已挂载、SW7 BIT3=I2S2、
 *       BIT1=功放开启。【未验证】音频链路尚未上板实测。
 */

#define LOG_TAG "APP_AUDIO"

#include "app_audio_demo.h"
#include "bsp_logger.h"
#include "bsp_audio.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief audio_play Shell 指令入口：播放指定 WAV 文件
 */
static int shell_audio_play(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: audio_play <path>  (e.g. 0:/music/test.wav)");
        return -1;
    }

    bsp_status_t ret = bsp_audio_play_wav(argv[1]);
    if (ret == BSP_BUSY)
    {
        log_w("playback already running, stop it first.");
        return -1;
    }
    if (ret != BSP_OK)
    {
        log_e("play %s failed! ret = %d", argv[1], ret);
        return -1;
    }

    return 0;
}

/**
 * @brief audio_stop Shell 指令入口：停止播放
 */
static int shell_audio_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bsp_status_t ret = bsp_audio_stop();
    if (ret != BSP_OK)
    {
        log_e("stop failed! ret = %d", ret);
        return -1;
    }

    return 0;
}

/**
 * @brief audio_vol Shell 指令入口：设置音量
 */
static int shell_audio_vol(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: audio_vol <0-100>");
        return -1;
    }

    int volume = atoi(argv[1]);

    bsp_status_t ret = bsp_audio_set_volume((uint8_t)((volume < 0) ? 0 : volume));
    if (ret != BSP_OK)
    {
        log_e("set volume failed! ret = %d", ret);
        return -1;
    }

    log_i("volume = %d", volume);
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 WAV 播放演示模块
 */
bsp_status_t app_audio_demo_init(void)
{
    bsp_status_t ret = bsp_audio_init();
    if (ret != BSP_OK)
    {
        return ret;
    }

    log_i("Audio Demo loaded. Try: audio_play 0:/music/test.wav");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_play, shell_audio_play, Play WAV file from SD card);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_stop, shell_audio_stop, Stop WAV playback);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_vol, shell_audio_vol, Set playback volume (0-100));
