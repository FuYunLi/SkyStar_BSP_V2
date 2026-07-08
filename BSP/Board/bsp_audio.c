/**
 * @file bsp_audio.c
 * @brief 板级音频子系统服务源文件
 */

#include "bsp_audio.h"
#include "bsp_bus.h"
#include "dev_es8388.h"
#include "dev_pca9555.h"
#include "port_i2s.h"
#include "bsp_file.h"
#include "bsp_logger.h"
#include <math.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BSP_AUDIO"

/* 双缓冲大小定义 (2048 样本/通道 * 2 通道 = 4096 字节/半缓冲 = 2048 半字) */
#define AUDIO_BUF_SIZE       4096U
#define AUDIO_HALF_BUF_SIZE  (AUDIO_BUF_SIZE / 2U)

/* 功放使能引脚配置：Port 0 Pin 0 */
#define AMP_PCA_PORT         0U
#define AMP_PCA_PIN          0U

/* 音频工作模式 */
typedef enum
{
    AUDIO_MODE_IDLE = 0,
    AUDIO_MODE_TONE,
    AUDIO_MODE_FILE
} audio_mode_t;

/* 全局唯一 ES8388 设备实例 */
static dev_es8388_t s_es8388;

/* DMA 循环双缓冲区 */
static uint16_t s_audio_dma_buf[AUDIO_BUF_SIZE];

/* 音频状态机变量 */
static audio_mode_t s_audio_mode = AUDIO_MODE_IDLE;

/* 双缓冲加载标记 */
static volatile bool s_half_empty = false;
static volatile bool s_cplt_empty = false;

/* -------------------------------------------------------------------------
 * 单音播放 (Tone) 上下文
 * ------------------------------------------------------------------------- */
static float s_tone_phase = 0.0f;
static float s_tone_phase_step = 0.0f;
static uint32_t s_tone_samples_played = 0;
static uint32_t s_tone_samples_total = 0;

/* -------------------------------------------------------------------------
 * 文件播放 (WAV) 上下文
 * ------------------------------------------------------------------------- */
static bsp_file_t s_audio_file;
static uint32_t s_wav_channels = 2;
static uint32_t s_wav_sample_rate = 44100;
static uint32_t s_wav_bits_per_sample = 16;
static uint32_t s_wav_data_remaining = 0;
static bool s_wav_file_opened = false;

/* 前向声明 */
static void s_fill_tone_buffer(uint16_t *buf, uint32_t len_samples);


/* -------------------------------------------------------------------------
 * 私有中断/DMA完成回调函数
 * 策略：
 *   - TONE 模式：直接在中断里填充正弦波（计算量小，安全）
 *   - FILE 模式：只设置标志位，主循环负责 SD 卡 IO（中断里不能做阻塞 IO）
 * ------------------------------------------------------------------------- */
static void s_i2s_half_cplt_cb(void)
{
    if (s_audio_mode == AUDIO_MODE_TONE)
    {
        /* 直接在 DMA 中断里重填前半缓冲，避免主循环延迟 */
        s_fill_tone_buffer(&s_audio_dma_buf[0], AUDIO_HALF_BUF_SIZE);
    }
    else
    {
        s_half_empty = true;
    }
}

static void s_i2s_cplt_cb(void)
{
    if (s_audio_mode == AUDIO_MODE_TONE)
    {
        /* 直接在 DMA 中断里重填后半缓冲，避免主循环延迟 */
        s_fill_tone_buffer(&s_audio_dma_buf[AUDIO_HALF_BUF_SIZE], AUDIO_HALF_BUF_SIZE);
    }
    else
    {
        s_cplt_empty = true;
    }
}

/* -------------------------------------------------------------------------
 * 私有填充缓冲函数
 * ------------------------------------------------------------------------- */

/**
 * @brief 产生单音正弦波填充指定的缓冲区半区
 */
static void s_fill_tone_buffer(uint16_t *buf, uint32_t len_samples)
{
    for (uint32_t i = 0; i < len_samples; i += 2)
    {
        if (s_tone_samples_played >= s_tone_samples_total)
        {
            /* 播放时间已到，填充静音 */
            buf[i] = 0;     /* 左声道 */
            buf[i + 1] = 0; /* 右声道 */
            continue;
        }

        /* 产生 16-bit 正弦波样值 */
        int16_t sample = (int16_t)(32767.0f * sinf(s_tone_phase));
        s_tone_phase += s_tone_phase_step;
        if (s_tone_phase >= 2.0f * 3.14159265f)
        {
            s_tone_phase -= 2.0f * 3.14159265f;
        }

        buf[i] = (uint16_t)sample;
        buf[i + 1] = (uint16_t)sample;

        s_tone_samples_played += 1; /* 按立体声帧计数 */
    }
}

/**
 * @brief 从 WAV 文件中读取并解码数据填充指定的缓冲区半区
 */
static void s_fill_file_buffer(uint16_t *buf, uint32_t len_samples)
{
    if (!s_wav_file_opened)
    {
        memset(buf, 0, len_samples * sizeof(uint16_t));
        return;
    }

    uint32_t bytes_per_sample = s_wav_bits_per_sample / 8;
    uint32_t bytes_needed = (len_samples / 2) * s_wav_channels * bytes_per_sample;
    
    if (bytes_needed > s_wav_data_remaining)
    {
        bytes_needed = s_wav_data_remaining;
    }

    if (bytes_needed == 0)
    {
        memset(buf, 0, len_samples * sizeof(uint16_t));
        return;
    }

    /* 临时读取缓冲区 */
    static uint8_t s_temp_read_buf[AUDIO_BUF_SIZE];
    if (bytes_needed > sizeof(s_temp_read_buf))
    {
        bytes_needed = sizeof(s_temp_read_buf);
    }

    uint32_t bytes_read = 0;
    bsp_status_t status = bsp_file_read(&s_audio_file, s_temp_read_buf, bytes_needed, &bytes_read);
    if (status != BSP_OK || bytes_read == 0)
    {
        s_wav_data_remaining = 0;
        memset(buf, 0, len_samples * sizeof(uint16_t));
        return;
    }

    s_wav_data_remaining -= bytes_read;

    /* 将读取的数据解码并填入 I2S 16-bit 左右声道缓冲中 */
    uint32_t num_frames = bytes_read / (s_wav_channels * bytes_per_sample);
    uint8_t *p_src = s_temp_read_buf;

    for (uint32_t i = 0; i < len_samples / 2; i++)
    {
        if (i < num_frames)
        {
            int16_t left = 0, right = 0;

            if (s_wav_bits_per_sample == 16)
            {
                if (s_wav_channels == 2)
                {
                    left = (int16_t)(p_src[0] | (p_src[1] << 8));
                    right = (int16_t)(p_src[2] | (p_src[3] << 8));
                    p_src += 4;
                }
                else /* 单声道 */
                {
                    left = (int16_t)(p_src[0] | (p_src[1] << 8));
                    right = left;
                    p_src += 2;
                }
            }
            else if (s_wav_bits_per_sample == 8)
            {
                if (s_wav_channels == 2)
                {
                    left = (int16_t)(((int16_t)p_src[0] - 128) << 8);
                    right = (int16_t)(((int16_t)p_src[1] - 128) << 8);
                    p_src += 2;
                }
                else
                {
                    left = (int16_t)(((int16_t)p_src[0] - 128) << 8);
                    right = left;
                    p_src += 1;
                }
            }

            buf[i * 2] = (uint16_t)left;
            buf[i * 2 + 1] = (uint16_t)right;
        }
        else
        {
            buf[i * 2] = 0;
            buf[i * 2 + 1] = 0;
        }
    }
}

/* ================================================================
 * 公开 API 实现
 * ================================================================ */

/**
 * @brief  初始化板级音频子系统
 */
bsp_status_t bsp_audio_init(void)
{
    /* 1. 初始化物理 I2C 总线，确保 ES8388 可访问 */
    bsp_status_t status = port_i2c_init(PORT_I2C_1);
    if (status != BSP_OK)
    {
        log_e("Audio I2C bus init failed");
        return status;
    }

    /* 2. 初始化 ES8388 编解码器芯片 */
    status = dev_es8388_init(&s_es8388, PORT_I2C_1, 0x20U);
    if (status != BSP_OK)
    {
        log_e("ES8388 Codec init failed");
        return status;
    }

    /* 2.5 配置 PCA9555 Port 0 Pin 0 (功放使能脚) 为输出并拉低以开启静音 */
    extern dev_pca9555_t g_pca_led;
    status = dev_pca9555_set_pin_dir(&g_pca_led, AMP_PCA_PORT, AMP_PCA_PIN, 0);
    if (status != BSP_OK)
    {
        log_e("Failed to config PCA9555 HT6872 amp pin dir");
        return status;
    }
    status = dev_pca9555_write_pin(&g_pca_led, AMP_PCA_PORT, AMP_PCA_PIN, DEV_PCA9555_RESET);
    if (status != BSP_OK)
    {
        log_e("Failed to disable HT6872 amp at init");
        return status;
    }

    /* 3. 注册 I2S 物理 DMA 中断回调 */
    port_i2s_register_callbacks(s_i2s_half_cplt_cb, s_i2s_cplt_cb);

    s_audio_mode = AUDIO_MODE_IDLE;
    log_i("Audio subsystem initialized successfully");
    return BSP_OK;
}

/**
 * @brief  播放单音（非阻塞）
 */
bsp_status_t bsp_audio_play_tone(uint32_t frequency, uint32_t duration_ms)
{
    if (frequency == 0 || duration_ms == 0)
    {
        return BSP_EINVAL;
    }

    /* 确保当前总线切换至 I2S 模式 */
    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_MODE_I2S);
    if (status != BSP_OK)
    {
        log_e("Failed to acquire I2S bus for tone playback");
        return status;
    }

    /* 重新配置 ES8388 确保工作于 DAC 播放状态 */
    status = dev_es8388_init(&s_es8388, PORT_I2C_1, 0x20U);
    if (status != BSP_OK)
    {
        log_e("Failed to re-init ES8388 for tone playback");
        return status;
    }

    /* 设置 tone 发生器参数 (假设采样率恒定为 44100Hz) */
    s_tone_phase = 0.0f;
    s_tone_phase_step = (2.0f * 3.14159265f * (float)frequency) / 44100.0f;
    s_tone_samples_total = (44100U * duration_ms) / 1000U;
    s_tone_samples_played = 0;

    s_audio_mode = AUDIO_MODE_TONE;

    /* 预填充两个缓冲半区 */
    s_fill_tone_buffer(&s_audio_dma_buf[0], AUDIO_HALF_BUF_SIZE);
    s_fill_tone_buffer(&s_audio_dma_buf[AUDIO_HALF_BUF_SIZE], AUDIO_HALF_BUF_SIZE);

    s_half_empty = false;
    s_cplt_empty = false;

    /* 开启 DMA 循环推流 */
    status = port_i2s_write_dma(s_audio_dma_buf, AUDIO_BUF_SIZE);
    if (status != BSP_OK)
    {
        s_audio_mode = AUDIO_MODE_IDLE;
        log_e("I2S DMA Transmit failed for tone playback");
        return status;
    }

    /* 开启功放 HT6872 */
    extern dev_pca9555_t g_pca_led;
    (void)dev_pca9555_write_pin(&g_pca_led, AMP_PCA_PORT, AMP_PCA_PIN, DEV_PCA9555_SET);

    log_i("Playing tone: %d Hz for %d ms", frequency, duration_ms);
    return BSP_OK;
}

/**
 * @brief  播放音频文件（支持 WAV 格式，非阻塞）
 */
bsp_status_t bsp_audio_play_file(const char *filepath)
{
    if (filepath == NULL)
    {
        return BSP_EINVAL;
    }

    if (s_audio_mode != AUDIO_MODE_IDLE)
    {
        bsp_audio_stop();
    }

    /* 打开音频文件 */
    bsp_status_t status = bsp_file_open(&s_audio_file, filepath, BSP_FILE_READ);
    if (status != BSP_OK)
    {
        log_e("Failed to open audio file: %s", filepath);
        return status;
    }
    s_wav_file_opened = true;

    /* 解析 WAV 头部格式 */
    uint8_t header_buf[16];
    uint32_t read_bytes = 0;
    
    /* 读取前 12 字节校验 */
    status = bsp_file_read(&s_audio_file, header_buf, 12, &read_bytes);
    if (status != BSP_OK || read_bytes != 12 || 
        memcmp(&header_buf[0], "RIFF", 4) != 0 || memcmp(&header_buf[8], "WAVE", 4) != 0)
    {
        log_e("Invalid WAV file format (missing RIFF/WAVE header)");
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
        return BSP_ERROR;
    }

    /* 逐个块搜索 "fmt " 和 "data" */
    s_wav_channels = 2;
    s_wav_sample_rate = 44100;
    s_wav_bits_per_sample = 16;
    s_wav_data_remaining = 0;

    while (1)
    {
        char chunk_hdr[4];
        uint32_t chunk_len = 0;

        if (bsp_file_read(&s_audio_file, chunk_hdr, 4, &read_bytes) != BSP_OK || read_bytes != 4)
        {
            break;
        }
        if (bsp_file_read(&s_audio_file, &chunk_len, 4, &read_bytes) != BSP_OK || read_bytes != 4)
        {
            break;
        }

        if (memcmp(chunk_hdr, "fmt ", 4) == 0)
        {
            uint16_t audio_format = 0;
            uint16_t num_channels = 0;
            uint32_t s_rate = 0;
            uint32_t byte_rate = 0;
            uint16_t block_align = 0;
            uint16_t bits = 0;

            bsp_file_read(&s_audio_file, &audio_format, 2, &read_bytes);
            bsp_file_read(&s_audio_file, &num_channels, 2, &read_bytes);
            bsp_file_read(&s_audio_file, &s_rate, 4, &read_bytes);
            bsp_file_read(&s_audio_file, &byte_rate, 4, &read_bytes);
            bsp_file_read(&s_audio_file, &block_align, 2, &read_bytes);
            bsp_file_read(&s_audio_file, &bits, 2, &read_bytes);

            s_wav_channels = num_channels;
            s_wav_sample_rate = s_rate;
            s_wav_bits_per_sample = bits;

            /* 只支持 PCM 格式 */
            if (audio_format != 1)
            {
                log_e("Unsupported WAV format: only uncompressed PCM is supported");
                bsp_file_close(&s_audio_file);
                s_wav_file_opened = false;
                return BSP_ERROR;
            }

            /* 跳过剩余部分 */
            if (chunk_len > 16)
            {
                uint8_t skip_buf[32];
                bsp_file_read(&s_audio_file, skip_buf, chunk_len - 16, &read_bytes);
            }
        }
        else if (memcmp(chunk_hdr, "data", 4) == 0)
        {
            s_wav_data_remaining = chunk_len;
            break;
        }
        else
        {
            /* 跳过不需要的块 */
            uint32_t remaining = chunk_len;
            uint8_t skip_buf[64];
            while (remaining > 0)
            {
                uint32_t to_read = (remaining > sizeof(skip_buf)) ? sizeof(skip_buf) : remaining;
                if (bsp_file_read(&s_audio_file, skip_buf, to_read, &read_bytes) != BSP_OK || read_bytes == 0)
                {
                    break;
                }
                remaining -= read_bytes;
            }
        }
    }

    if (s_wav_data_remaining == 0)
    {
        log_e("WAV file contains no audio data chunk");
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
        return BSP_ERROR;
    }

    log_i("WAV file parsed: rate=%dHz, channels=%d, bits=%d", 
          s_wav_sample_rate, s_wav_channels, s_wav_bits_per_sample);

    /* 申请占有 I2S 总线 */
    status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_MODE_I2S);
    if (status != BSP_OK)
    {
        log_e("Failed to acquire I2S bus for file playback");
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
        return status;
    }

    /* 动态设置 I2S 物理采样率 */
    status = port_i2s_set_sample_rate(s_wav_sample_rate);
    if (status != BSP_OK)
    {
        log_e("Failed to configure I2S sample rate for file playback");
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
        return status;
    }

    /* 重新初始化 ES8388 并动态调整 I2S 时钟 (只重新初始化 ES8388 以对齐总线) */
    status = dev_es8388_init(&s_es8388, PORT_I2C_1, 0x20U);

    if (status != BSP_OK)
    {
        log_e("Failed to re-init ES8388 for file playback");
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
        return status;
    }

    s_audio_mode = AUDIO_MODE_FILE;

    /* 填充双缓冲 */
    s_fill_file_buffer(&s_audio_dma_buf[0], AUDIO_HALF_BUF_SIZE);
    s_fill_file_buffer(&s_audio_dma_buf[AUDIO_HALF_BUF_SIZE], AUDIO_HALF_BUF_SIZE);

    s_half_empty = false;
    s_cplt_empty = false;

    /* 启动 I2S DMA 传输 */
    status = port_i2s_write_dma(s_audio_dma_buf, AUDIO_BUF_SIZE);
    if (status != BSP_OK)
    {
        s_audio_mode = AUDIO_MODE_IDLE;
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
        log_e("I2S DMA Transmit failed for file playback");
        return status;
    }

    /* 开启功放 HT6872 */
    extern dev_pca9555_t g_pca_led;
    (void)dev_pca9555_write_pin(&g_pca_led, AMP_PCA_PORT, AMP_PCA_PIN, DEV_PCA9555_SET);

    log_i("Playing audio file: %s", filepath);
    return BSP_OK;
}

/**
 * @brief  停止音频播放，释放音频总线
 */
bsp_status_t bsp_audio_stop(void)
{
    if (s_audio_mode == AUDIO_MODE_IDLE)
    {
        return BSP_OK;
    }

    /* 关闭功放 HT6872，防止爆音 */
    extern dev_pca9555_t g_pca_led;
    (void)dev_pca9555_write_pin(&g_pca_led, AMP_PCA_PORT, AMP_PCA_PIN, DEV_PCA9555_RESET);

    /* 停止 I2S DMA */
    port_i2s_stop();

    if (s_audio_mode == AUDIO_MODE_FILE && s_wav_file_opened)
    {
        bsp_file_close(&s_audio_file);
        s_wav_file_opened = false;
    }

    s_audio_mode = AUDIO_MODE_IDLE;
    s_half_empty = false;
    s_cplt_empty = false;

    /* 释放总线，归还给 SPI */
    bsp_bus_release(BSP_BUS_SPI2_I2S2);

    log_i("Audio playback stopped, bus released to SPI");
    return BSP_OK;
}

/**
 * @brief  设置播放音量
 */
bsp_status_t bsp_audio_set_volume(uint8_t volume)
{
    return dev_es8388_set_volume(&s_es8388, volume);
}

/**
 * @brief  静音音频输出
 */
bsp_status_t bsp_audio_mute(void)
{
    return dev_es8388_mute(&s_es8388);
}

/**
 * @brief  取消静音音频输出
 */
bsp_status_t bsp_audio_unmute(void)
{
    return dev_es8388_unmute(&s_es8388);
}

/**
 * @brief  音频服务轮询更新任务
 * @note   TONE 模式的缓冲区填充已移至 DMA 中断回调内直接完成。
 *         本函数仅负责：
 *           - TONE: 检测播放时间终点并触发停止
 *           - FILE: 充充充充（SD 卡 IO 必须在主循环）
 */
void bsp_audio_update(void)
{
    if (s_audio_mode == AUDIO_MODE_IDLE)
    {
        return;
    }

    /* TONE 模式：缓冲区已由 DMA ISR 填充，本处仅做终止检测 */
    if (s_audio_mode == AUDIO_MODE_TONE)
    {
        if (s_tone_samples_played >= s_tone_samples_total)
        {
            log_i("Tone playback completed");
            bsp_audio_stop();
        }
        return;
    }

    /* FILE 模式：必须在主循环处理，SD卡IO不能在中断里执行 */
    if (s_half_empty)
    {
        s_half_empty = false;
        s_fill_file_buffer(&s_audio_dma_buf[0], AUDIO_HALF_BUF_SIZE);
        if (s_wav_data_remaining == 0)
        {
            log_i("WAV file playback completed");
            bsp_audio_stop();
            return;
        }
    }

    if (s_cplt_empty)
    {
        s_cplt_empty = false;
        s_fill_file_buffer(&s_audio_dma_buf[AUDIO_HALF_BUF_SIZE], AUDIO_HALF_BUF_SIZE);
        if (s_wav_data_remaining == 0)
        {
            log_i("WAV file playback completed");
            bsp_audio_stop();
            return;
        }
    }
}

/**
 * @brief  查询音频当前是否正在播放
 */
bool bsp_audio_is_playing(void)
{
    return (s_audio_mode != AUDIO_MODE_IDLE);
}
