/**
 * @file bsp_audio.c
 * @brief 板级音频服务层实现
 * @note 双缓冲调度模型：
 *       DMA 播放 buf[active] → 完成中断置交换标志（ISR 只做这一件事）
 *       → 主循环 bsp_audio_process 检测标志：重启 DMA 播放预填好的
 *       buf[active^1]，同时从文件回填刚播完的 buf[active]。
 *       单声道 WAV 在回填时原位扩展为双声道（从后向前覆盖，避免
 *       额外缓冲）。
 *       【未验证】音频链路尚未上板实测。
 */

#define LOG_TAG "BSP_AUDIO"

#include "bsp_audio.h"
#include "bsp_logger.h"
#include "bsp_file.h"
#include "dev_es8388.h"
#include "port_i2s.h"
#include <string.h>

/* ================================================================
 * 私有宏定义与常量
 * ================================================================ */

#define AUDIO_BUF_SAMPLES   (2048U)  /* 单缓冲 16 位采样元素数（4KB） */
#define AUDIO_BUF_BYTES     (AUDIO_BUF_SAMPLES * 2U)
#define AUDIO_DEFAULT_VOL   (60U)    /* 默认音量 */
#define AUDIO_RATE_MIN      (8000U)
#define AUDIO_RATE_MAX      (48000U)

/* WAV 头部常量 */
#define WAV_FMT_PCM         (1U)
#define WAV_BITS_16         (16U)

/* ================================================================
 * 私有变量
 * ================================================================ */

/* 双缓冲静态分配：常规 SRAM 区域，DMA 可达（勿放 CCM） */
static uint16_t s_audio_buf[2][AUDIO_BUF_SAMPLES];
static uint16_t s_buf_samples[2];        /* 各缓冲有效采样数，0 为 EOF 标记 */

static bsp_file_t s_audio_file;          /* 播放文件句柄 */
static bool s_file_open;
static bool s_playing;
static bool s_stereo;                    /* 源 WAV 声道数 */
static uint8_t s_active_buf;             /* DMA 正在播放的缓冲下标 */

/* 交换标志：DMA 完成中断置位，主循环消费 */
static volatile bool s_swap_request;

static uint8_t s_volume = AUDIO_DEFAULT_VOL;
static bool s_audio_ready;               /* dev_es8388 初始化完成标志 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 从 WAV 文件头解析格式并定位数据块
 * @param[out] sample_rate 采样率回传
 * @retval BSP_OK 解析成功；BSP_ERROR 格式不支持
 */
static bsp_status_t s_wav_parse_header(uint32_t *sample_rate)
{
    uint8_t chunk_hdr[8];
    uint32_t got = 0U;
    bool fmt_ok = false;

    *sample_rate = 0U;

    /* RIFF 容器头：'RIFF' + 文件大小 + 'WAVE' 共 12 字节 */
    if ((bsp_file_read(&s_audio_file, chunk_hdr, 12U, &got) != BSP_OK) ||
        (got != 12U) || (chunk_hdr[0] != 'R') || (chunk_hdr[1] != 'I') ||
        (chunk_hdr[2] != 'F') || (chunk_hdr[3] != 'F') ||
        (chunk_hdr[8] != 'W') || (chunk_hdr[9] != 'A') ||
        (chunk_hdr[10] != 'V') || (chunk_hdr[11] != 'E'))
    {
        return BSP_ERROR;
    }

    /* 逐块扫描子块，直到定位 data 块（fmt 块必须先于 data 出现） */
    for (;;)
    {
        if ((bsp_file_read(&s_audio_file, chunk_hdr, 8U, &got) != BSP_OK) || (got != 8U))
        {
            return BSP_ERROR;
        }

        uint32_t chunk_size = (uint32_t)chunk_hdr[4] | ((uint32_t)chunk_hdr[5] << 8) |
                              ((uint32_t)chunk_hdr[6] << 16) | ((uint32_t)chunk_hdr[7] << 24);

        if ((chunk_hdr[0] == 'f') && (chunk_hdr[1] == 'm') && (chunk_hdr[2] == 't') && (chunk_hdr[3] == ' '))
        {
            uint8_t fmt_buf[16];
            uint32_t fmt_size = (chunk_size < sizeof(fmt_buf)) ? chunk_size : sizeof(fmt_buf);

            if ((bsp_file_read(&s_audio_file, fmt_buf, fmt_size, &got) != BSP_OK) ||
                (got != fmt_size) || (fmt_size < 16U))
            {
                return BSP_ERROR;
            }

            /* 跳过 fmt 块声明长度超出已读部分的填充 */
            if (chunk_size > fmt_size)
            {
                uint8_t skip_byte;
                for (uint32_t i = 0U; i < (chunk_size - fmt_size); i++)
                {
                    if (bsp_file_read(&s_audio_file, &skip_byte, 1U, &got) != BSP_OK)
                    {
                        return BSP_ERROR;
                    }
                }
            }

            uint16_t audio_format = (uint16_t)((uint16_t)fmt_buf[0] | ((uint16_t)fmt_buf[1] << 8));
            uint16_t channels = (uint16_t)((uint16_t)fmt_buf[2] | ((uint16_t)fmt_buf[3] << 8));
            *sample_rate = (uint32_t)fmt_buf[4] | ((uint32_t)fmt_buf[5] << 8) |
                           ((uint32_t)fmt_buf[6] << 16) | ((uint32_t)fmt_buf[7] << 24);
            uint16_t bits = (uint16_t)((uint16_t)fmt_buf[14] | ((uint16_t)fmt_buf[15] << 8));

            if ((audio_format != WAV_FMT_PCM) || (bits != WAV_BITS_16) ||
                ((channels != 1U) && (channels != 2U)) ||
                (*sample_rate < AUDIO_RATE_MIN) || (*sample_rate > AUDIO_RATE_MAX))
            {
                return BSP_ERROR;
            }

            s_stereo = (channels == 2U);
            fmt_ok = true;
        }
        else if ((chunk_hdr[0] == 'd') && (chunk_hdr[1] == 'a') && (chunk_hdr[2] == 't') && (chunk_hdr[3] == 'a'))
        {
            if (!fmt_ok)
            {
                return BSP_ERROR;
            }

            return BSP_OK;
        }
        else
        {
            /* 跳过不认识的子块（LIST/CUE 等），按块长逐字节丢弃 */
            uint8_t skip_byte;
            for (uint32_t i = 0U; i < chunk_size; i++)
            {
                if (bsp_file_read(&s_audio_file, &skip_byte, 1U, &got) != BSP_OK)
                {
                    return BSP_ERROR;
                }
            }
        }
    }
}

/**
 * @brief 从文件回填一个缓冲区（含单声道原位扩展）
 * @param idx 缓冲下标
 * @note 单声道扩展技巧：源数据先读入缓冲区前半段，再从后向前
 *       逐样本复制两份——目标区下标恒不小于源区，无覆盖风险
 */
static void s_fill_buffer(uint8_t idx)
{
    uint32_t got = 0U;

    if (!s_stereo)
    {
        /* 单声道：按采样数读取（16 位），再原位展开为 L/R 交错 */
        uint32_t mono_bytes = AUDIO_BUF_SAMPLES;  /* 单声道半容量字节数 */
        if (bsp_file_read(&s_audio_file, s_audio_buf[idx], mono_bytes, &got) != BSP_OK)
        {
            got = 0U;
        }

        uint32_t mono_samples = got / 2U;
        const uint16_t *src = s_audio_buf[idx];

        for (uint32_t i = mono_samples; i > 0U; i--)
        {
            s_audio_buf[idx][(2U * i) - 2U] = src[i - 1U];
            s_audio_buf[idx][(2U * i) - 1U] = src[i - 1U];
        }

        s_buf_samples[idx] = (uint16_t)(mono_samples * 2U);
    }
    else
    {
        if (bsp_file_read(&s_audio_file, s_audio_buf[idx], AUDIO_BUF_BYTES, &got) != BSP_OK)
        {
            got = 0U;
        }

        s_buf_samples[idx] = (uint16_t)(got / 2U);
    }

    /* EOF 或尾部不足一帧：以静音补齐，缓冲标记为最后一块 */
    for (uint32_t i = s_buf_samples[idx]; i < AUDIO_BUF_SAMPLES; i++)
    {
        s_audio_buf[idx][i] = 0U;
    }
}

/**
 * @brief I2S 发送完成回调（ISR 上下文）：仅置交换标志
 */
static void s_audio_tx_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx)
{
    (void)bus_id;
    (void)result;
    (void)user_ctx;

    if (s_playing)
    {
        s_swap_request = true;
    }
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化音频服务
 */
bsp_status_t bsp_audio_init(void)
{
    bsp_status_t ret = dev_es8388_init();
    if (ret != BSP_OK)
    {
        log_e("ES8388 init failed! ret = %d", ret);
        return ret;
    }

    (void)dev_es8388_set_volume(s_volume);
    s_audio_ready = true;

    log_i("Audio ready. SW7: BIT3=I2S2, BIT1=AMP-ON.");
    return BSP_OK;
}

/**
 * @brief 播放 WAV 文件
 */
bsp_status_t bsp_audio_play_wav(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }

    if (s_playing)
    {
        return BSP_BUSY;
    }

    if (!s_audio_ready)
    {
        return BSP_ERROR;
    }

    /* 1. 打开文件并解析格式 */
    s_audio_file.type = BSP_FILE_TYPE_UNKNOWN;
    bsp_status_t ret = bsp_file_open(&s_audio_file, path, BSP_FILE_READ);
    if (ret != BSP_OK)
    {
        log_e("open %s failed! ret = %d", path, ret);
        return BSP_ERROR;
    }
    s_file_open = true;

    uint32_t sample_rate = 0U;
    ret = s_wav_parse_header(&sample_rate);
    if (ret != BSP_OK)
    {
        log_e("unsupported WAV format (%s)", path);
        (void)bsp_file_close(&s_audio_file);
        s_file_open = false;
        return BSP_ERROR;
    }

    /* 2. 建立 I2S 通道并启动 DAC 路径（此刻起 SPI2 被接管） */
    ret = port_i2s_init(PORT_I2S_2, sample_rate);
    if (ret != BSP_OK)
    {
        log_e("I2S init failed! ret = %d", ret);
        (void)bsp_file_close(&s_audio_file);
        s_file_open = false;
        return ret;
    }

    ret = dev_es8388_start();
    if (ret != BSP_OK)
    {
        log_e("ES8388 start failed! ret = %d", ret);
        (void)port_i2s_deinit(PORT_I2S_2);
        (void)bsp_file_close(&s_audio_file);
        s_file_open = false;
        return ret;
    }

    /* 3. 预填双缓冲后启动 DMA 播放链 */
    s_fill_buffer(0U);
    s_fill_buffer(1U);

    if (s_buf_samples[0] == 0U)
    {
        log_e("empty audio data (%s)", path);
        (void)port_i2s_deinit(PORT_I2S_2);
        (void)dev_es8388_stop();
        (void)bsp_file_close(&s_audio_file);
        s_file_open = false;
        return BSP_ERROR;
    }

    s_active_buf = 0U;
    s_swap_request = false;
    s_playing = true;

    ret = port_i2s_write_dma(PORT_I2S_2, s_audio_buf[0], s_buf_samples[0], s_audio_tx_cb, NULL);
    if (ret != BSP_OK)
    {
        s_playing = false;
        (void)port_i2s_deinit(PORT_I2S_2);
        (void)dev_es8388_stop();
        (void)bsp_file_close(&s_audio_file);
        s_file_open = false;
        return ret;
    }

    log_i("playing %s (%luHz %s)", path, (unsigned long)sample_rate, s_stereo ? "stereo" : "mono");
    return BSP_OK;
}

/**
 * @brief 停止播放并恢复 SPI2
 */
bsp_status_t bsp_audio_stop(void)
{
    if (!s_playing)
    {
        return BSP_OK;
    }

    s_playing = false;
    s_swap_request = false;

    (void)port_i2s_stop(PORT_I2S_2);
    (void)dev_es8388_stop();
    (void)port_i2s_deinit(PORT_I2S_2);

    if (s_file_open)
    {
        (void)bsp_file_close(&s_audio_file);
        s_file_open = false;
    }

    log_i("playback stopped, SPI2 restored.");
    return BSP_OK;
}

/**
 * @brief 设置播放音量
 */
bsp_status_t bsp_audio_set_volume(uint8_t volume)
{
    s_volume = volume;

    return dev_es8388_set_volume(volume);
}

/**
 * @brief 播放泵：交换缓冲并回填
 */
void bsp_audio_process(void)
{
    if (!s_playing || !s_swap_request)
    {
        return;
    }

    s_swap_request = false;

    uint8_t next = (uint8_t)(s_active_buf ^ 1U);

    /* 预填缓冲为 EOF 标记（0 采样）：当前曲目播完，收尾 */
    if (s_buf_samples[next] == 0U)
    {
        (void)bsp_audio_stop();
        return;
    }

    /* 立即重启 DMA 播放另一块预填缓冲 */
    bsp_status_t ret = port_i2s_write_dma(PORT_I2S_2, s_audio_buf[next], s_buf_samples[next],
                                          s_audio_tx_cb, NULL);
    if (ret != BSP_OK)
    {
        log_e("dma restart failed! ret = %d", ret);
        (void)bsp_audio_stop();
        return;
    }

    /* 回填刚播完的缓冲，供下下次交换使用 */
    uint8_t finished = s_active_buf;
    s_active_buf = next;
    s_fill_buffer(finished);
}
