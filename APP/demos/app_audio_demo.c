/**
 * @file app_audio_demo.c
 * @brief 自检演示模块——音频子系统 Shell 自检指令实现
 * @note  M30：audio_bus_switch 验证 SPI2/I2S2 复用总线的软件仲裁切换；
 *        M31：audio_init/audio_set_vol/audio_pa 验证 ES8388 与 HT6872 链路。
 */

#define LOG_TAG "APP_AUDIO"

#include "app_audio_demo.h"
#include "bsp_audio.h"
#include "bsp_bus.h"
#include "bsp_logger.h"
#include "dev_es8388.h"
#include "dev_ht6872.h"
#include "port_i2s.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================================================
 * 私有函数声明与实现
 * ================================================================ */

/**
 * @brief 手动切换 SPI2/I2S2 复用总线归属
 * @note  用法：audio_bus_switch [i2s|spi]
 */
static void shell_audio_bus_switch(int argc, char *argv[])
{
    bsp_bus_owner_t target;
    if (argc >= 2 && strcmp(argv[1], "i2s") == 0)
    {
        target = BSP_BUS_OWNER_I2S2;
    }
    else if (argc >= 2 && strcmp(argv[1], "spi") == 0)
    {
        target = BSP_BUS_OWNER_SPI2;
    }
    else
    {
        printf("Usage: audio_bus_switch [i2s|spi]\r\n");
        printf("Current owner: %s\r\n",
               (bsp_bus_current(BSP_BUS_SPI2_I2S2) == BSP_BUS_OWNER_I2S2) ? "I2S2"
               : (bsp_bus_current(BSP_BUS_SPI2_I2S2) == BSP_BUS_OWNER_SPI2) ? "SPI2"
                                                                            : "NONE");
        return;
    }

    /* 手动切换属接管语义：先释放当前归属方的占用权再申请，
     * 切换链路内部会自动挂起 IMU / 停止 I2S 流，接管是安全的 */
    bsp_bus_owner_t current = bsp_bus_current(BSP_BUS_SPI2_I2S2);
    if (current != BSP_BUS_OWNER_NONE && current != target)
    {
        (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, current);
    }

    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, target);
    if (status != BSP_OK)
    {
        printf("Bus switch failed, ret = %d\r\n", status);
        return;
    }
    printf("Bus switched to %s OK\r\n", (target == BSP_BUS_OWNER_I2S2) ? "I2S2" : "SPI2");
}

/**
 * @brief 初始化音频编解码链路（自动接管 I2S2 总线）
 * @note  序列：仲裁取得 I2S2 → I2S2 外设初始化 → ES8388 上电序列 →
 *        功放通道注册（默认静音）。
 */
static void shell_audio_init(void)
{
    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
    if (status != BSP_OK)
    {
        printf("I2S2 bus acquire failed, ret = %d\r\n", status);
        return;
    }

    status = port_i2s_init(PORT_I2S_1);
    if (status != BSP_OK)
    {
        printf("I2S2 init failed, ret = %d\r\n", status);
        return;
    }

    status = dev_es8388_init();
    if (status != BSP_OK)
    {
        printf("ES8388 init failed, ret = %d\r\n", status);
        return;
    }

    status = dev_ht6872_init();
    if (status != BSP_OK)
    {
        printf("HT6872 amp init failed, ret = %d\r\n", status);
        return;
    }
    printf("Audio codec chain init OK (ES8388 + HT6872)\r\n");
}

/**
 * @brief 设置 DAC 音量（0-100）
 * @note  用法：audio_set_vol <0-100>
 */
static void shell_audio_set_vol(int argc, char *argv[])
{
    if (argc < 2)
    {
        uint8_t vol = 0;
        if (dev_es8388_get_dac_volume(&vol) == BSP_OK)
        {
            printf("Current volume: %u\r\n", vol);
        }
        else
        {
            printf("Read volume failed\r\n");
        }
        return;
    }

    int vol = atoi(argv[1]);
    if (vol < 0 || vol > 100)
    {
        printf("Usage: audio_set_vol <0-100>\r\n");
        return;
    }
    bsp_status_t status = dev_es8388_set_dac_volume((uint8_t)vol);
    if (status != BSP_OK)
    {
        printf("Set volume failed, ret = %d\r\n", status);
        return;
    }
    printf("Volume set to %d OK\r\n", vol);
}

/**
 * @brief 功放使能手动控制（验证 PCA9555 BIT1 电平）
 * @note  用法：audio_pa [on|off]
 */
static void shell_audio_pa(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: audio_pa [on|off]\r\n");
        return;
    }
    bool en = (strcmp(argv[1], "on") == 0);
    bsp_status_t status = dev_ht6872_enable(en);
    if (status != BSP_OK)
    {
        printf("PA control failed, ret = %d\r\n", status);
        return;
    }
    printf("PA %s OK\r\n", en ? "enabled" : "disabled");
}

/**
 * @brief 播放 TF 卡中的 WAV 文件（16-bit PCM / 44.1kHz / 单或立体声）
 * @note  用法：play_wav <path>，如 play_wav 0:/music/test.wav
 */
static void shell_play_wav(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: play_wav <path>\r\n");
        return;
    }
    bsp_status_t status = bsp_audio_play(argv[1]);
    if (status != BSP_OK)
    {
        printf("Play failed, ret = %d\r\n", status);
        return;
    }
    printf("Playing %s\r\n", argv[1]);
}

/**
 * @brief 停止播放
 */
static void shell_play_stop(void)
{
    bsp_status_t status = bsp_audio_stop();
    printf("Stop %s\r\n", (status == BSP_OK) ? "OK" : "failed");
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t app_audio_demo_init(void)
{
    log_i("Audio demo module registered (M30 bus arbitration, M31 codec)");
    return BSP_OK;
}

/* ================================================================
 * Shell 命令导出
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 audio_bus_switch, shell_audio_bus_switch, Switch SPI2/I2S2 shared bus [i2s|spi]);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN,
                 audio_init, shell_audio_init, Init ES8388 codec and HT6872 amp);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 audio_set_vol, shell_audio_set_vol, Set DAC volume 0-100);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 audio_pa, shell_audio_pa, Enable/disable HT6872 amp [on|off]);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 play_wav, shell_play_wav, Play WAV file from TF card);

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN,
                 play_stop, shell_play_stop, Stop WAV playback);
