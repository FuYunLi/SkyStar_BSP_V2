/**
 * @file app_audio_demo.c
 * @brief 自检演示模块——板载音频子系统 Shell 自检指令实现
 */

#define LOG_TAG "APP_AUDIO"

#include "app_audio_demo.h"
#include "bsp_audio.h"
#include "bsp_bus.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int shell_audio_bus_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bsp_bus_mode_t mode = bsp_bus_get_mode(BSP_BUS_SPI2_I2S2);
    log_i("Current SPI2/I2S2 Bus Mode: %s", (mode == BSP_BUS_MODE_SPI) ? "SPI" : "I2S");
    return 0;
}

static int shell_audio_bus_switch(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: audio_bus_switch <spi|i2s>");
        return -1;
    }

    bsp_status_t status;
    if (strcmp(argv[1], "spi") == 0)
    {
        status = bsp_bus_release(BSP_BUS_SPI2_I2S2);
        if (status == BSP_OK)
        {
            log_i("Successfully switched SPI2/I2S2 bus to SPI mode.");
        }
        else
        {
            log_e("Failed to switch SPI2/I2S2 bus to SPI mode, ret = %d", status);
        }
    }
    else if (strcmp(argv[1], "i2s") == 0)
    {
        status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_MODE_I2S);
        if (status == BSP_OK)
        {
            log_i("Successfully switched SPI2/I2S2 bus to I2S mode.");
        }
        else
        {
            log_e("Failed to switch SPI2/I2S2 bus to I2S mode, ret = %d", status);
        }
    }
    else
    {
        log_e("Unknown mode: %s. Use spi or i2s.", argv[1]);
        return -2;
    }

    return 0;
}

static int shell_audio_init(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bsp_status_t status = bsp_audio_init();
    if (status == BSP_OK)
    {
        log_i("Audio subsystem initialized successfully.");
    }
    else
    {
        log_e("Failed to initialize audio subsystem, ret = %d", status);
    }
    return 0;
}

static int shell_audio_play_tone(int argc, char *argv[])
{
    if (argc < 3)
    {
        log_e("Usage: audio_play_tone <freq_hz> <duration_ms>");
        return -1;
    }

    uint32_t freq = (uint32_t)atoi(argv[1]);
    uint32_t duration = (uint32_t)atoi(argv[2]);

    bsp_status_t status = bsp_audio_play_tone(freq, duration);
    if (status == BSP_OK)
    {
        log_i("Tone playback started: %d Hz for %d ms", freq, duration);
    }
    else
    {
        log_e("Failed to play tone, ret = %d", status);
    }
    return 0;
}

static int shell_audio_play_file(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: audio_play_file <filepath>");
        return -1;
    }

    bsp_status_t status = bsp_audio_play_file(argv[1]);
    if (status == BSP_OK)
    {
        log_i("Audio file playback started: %s", argv[1]);
    }
    else
    {
        log_e("Failed to play audio file, ret = %d", status);
    }
    return 0;
}

static int shell_audio_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bsp_status_t status = bsp_audio_stop();
    if (status == BSP_OK)
    {
        log_i("Audio playback stopped.");
    }
    else
    {
        log_e("Failed to stop audio, ret = %d", status);
    }
    return 0;
}

static int shell_audio_set_vol(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: audio_set_vol <volume 0-100>");
        return -1;
    }

    uint8_t vol = (uint8_t)atoi(argv[1]);
    if (vol > 100)
    {
        vol = 100;
    }

    bsp_status_t status = bsp_audio_set_volume(vol);
    if (status == BSP_OK)
    {
        log_i("Volume set to %d%%", vol);
    }
    else
    {
        log_e("Failed to set volume, ret = %d", status);
    }
    return 0;
}

static int shell_audio_mute(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bsp_status_t status = bsp_audio_mute();
    if (status == BSP_OK)
    {
        log_i("Audio output muted.");
    }
    else
    {
        log_e("Failed to mute audio, ret = %d", status);
    }
    return 0;
}

static int shell_audio_unmute(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bsp_status_t status = bsp_audio_unmute();
    if (status == BSP_OK)
    {
        log_i("Audio output unmuted.");
    }
    else
    {
        log_e("Failed to unmute audio, ret = %d", status);
    }
    return 0;
}

bsp_status_t app_audio_demo_init(void)
{
    log_i("Audio Shell Demo loaded.");
    return BSP_OK;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, audio_bus_status, shell_audio_bus_status, get current SPI2/I2S2 bus mode);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_bus_switch, shell_audio_bus_switch, switch SPI2/I2S2 bus mode);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, audio_init, shell_audio_init, initialize audio subsystem);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_play_tone, shell_audio_play_tone, play raw sine tone);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_play_file, shell_audio_play_file, play wav file from fs);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, audio_stop, shell_audio_stop, stop audio playback);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, audio_set_vol, shell_audio_set_vol, set audio volume 0-100);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, audio_mute, shell_audio_mute, mute audio output);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, audio_unmute, shell_audio_unmute, unmute audio output);
