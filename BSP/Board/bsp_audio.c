/**
 * @file bsp_audio.c
 * @brief 板级音频播放服务实现源文件
 * @note  双缓冲（2 x 2048 样本）循环 DMA 播放：I2S 半传输/全传输回调只置
 *        volatile 标志，FatFS 读取在 MultiTimer 周期任务中执行，避免在
 *        ISR 上下文重入文件系统。WAV 解析按 RIFF chunk 遍历，不硬编码头长。
 */

#define LOG_TAG "BSP_AUDIO"

#include "bsp_audio.h"
#include "bsp_bus.h"
#include "bsp_file.h"
#include "bsp_logger.h"
#include "dev_es8388.h"
#include "dev_ht6872.h"
#include "port_i2s.h"
#include "MultiTimer.h"
#include <string.h>

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* 双缓冲：每半区 2048 个 16-bit 样本（4KB），DMA 循环总长 4096 样本 */
#define AUDIO_HALF_SAMPLES  (2048U)
#define AUDIO_BUF_SAMPLES   (AUDIO_HALF_SAMPLES * 2U)

/* 填充任务周期：半区播放时长约 2048/44100 ≈ 46ms（立体声），5ms 轮询余量充足 */
#define AUDIO_FILL_PERIOD_MS (5U)

/* WAV 约束 */
#define WAV_SAMPLE_RATE     (44100U)
#define WAV_BITS_PER_SAMPLE (16U)

/* ================================================================
 * 私有类型
 * ================================================================ */

/* WAV fmt 块（PCM 基本段） */
typedef struct
{
    uint16_t format;        /* 1 = PCM */
    uint16_t channels;      /* 1 = 单声道, 2 = 立体声 */
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} wav_fmt_t;

/* ================================================================
 * 私有变量
 * ================================================================ */

static uint16_t s_audio_buf[2][AUDIO_HALF_SAMPLES];   /* 8KB，禁止上栈 */
static bsp_file_t s_audio_file;
static uint32_t s_data_remaining;                     /* data 块剩余字节数 */
static uint8_t s_channels;
static volatile bool s_half_pending;
static volatile bool s_full_pending;
static volatile bool s_playing;
static bool s_chain_inited;                           /* codec 链路是否已初始化 */
static MultiTimer s_fill_timer;

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

/**
 * @brief I2S 半传输回调（ISR 上下文，仅置标志）
 */
static void audio_half_cb(void *user_ctx)
{
    (void)user_ctx;
    s_half_pending = true;
}

/**
 * @brief I2S 全传输回调（ISR 上下文，仅置标志）
 */
static void audio_full_cb(void *user_ctx)
{
    (void)user_ctx;
    s_full_pending = true;
}

/**
 * @brief 向指定半区填充数据；文件尾补零
 * @return true 该半区已到达文件尾
 */
static bool audio_fill_half(uint16_t *half)
{
    uint8_t *dst = (uint8_t *)half;
    uint32_t want = AUDIO_HALF_SAMPLES * 2U;
    uint32_t got = 0;
    bool eof = false;

    if (s_channels == 1U)
    {
        /* 单声道：先读 1/2 容量的原始样本，再复制到左右声道 */
        uint16_t mono[AUDIO_HALF_SAMPLES / 2U];
        uint32_t mono_bytes = want / 2U;
        if (mono_bytes > s_data_remaining)
        {
            mono_bytes = s_data_remaining;
        }
        if (bsp_file_read(&s_audio_file, mono, mono_bytes, &got) != BSP_OK)
        {
            got = 0;
        }
        s_data_remaining -= got;
        uint32_t samples = got / 2U;
        for (uint32_t i = 0; i < samples; i++)
        {
            half[2U * i] = mono[i];
            half[2U * i + 1U] = mono[i];
        }
        if (got < mono_bytes)
        {
            memset(&half[2U * samples], 0, want - 2U * samples);
            eof = true;
        }
    }
    else
    {
        uint32_t bytes = want;
        if (bytes > s_data_remaining)
        {
            bytes = s_data_remaining;
        }
        if (bsp_file_read(&s_audio_file, dst, bytes, &got) != BSP_OK)
        {
            got = 0;
        }
        s_data_remaining -= got;
        if (got < want)
        {
            memset(dst + got, 0, want - got);
            eof = true;
        }
    }
    return eof;
}

/**
 * @brief 填充任务（MultiTimer 上下文，自续期）
 */
static void audio_fill_timer_cb(MultiTimer *timer, void *user_data)
{
    (void)timer;
    (void)user_data;

    if (!s_playing)
    {
        return;
    }

    bool eof = false;
    if (s_half_pending)
    {
        s_half_pending = false;
        eof |= audio_fill_half(s_audio_buf[0]);
    }
    if (s_full_pending)
    {
        s_full_pending = false;
        eof |= audio_fill_half(s_audio_buf[1]);
    }

    if (eof && s_data_remaining == 0U && !s_half_pending && !s_full_pending)
    {
        (void)bsp_audio_stop();
        log_i("Playback finished");
        return;
    }

    multiTimerStart(&s_fill_timer, AUDIO_FILL_PERIOD_MS, audio_fill_timer_cb, NULL);
}

/**
 * @brief 停止播放的内部完整收尾（释放文件、停流、静音、释放占用权）
 */
static void audio_cleanup(void)
{
    (void)port_i2s_stop(PORT_I2S_1);
    (void)dev_es8388_stop_dac();
    (void)dev_ht6872_enable(false);
    (void)bsp_file_close(&s_audio_file);
    (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
    s_playing = false;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t bsp_audio_play(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }
    if (s_playing)
    {
        return BSP_BUSY;
    }

    /* 1. 确保总线与 codec 链路就绪（幂等） */
    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
    if (status != BSP_OK)
    {
        return status;
    }
    if (!s_chain_inited)
    {
        status = port_i2s_init(PORT_I2S_1);
        if (status == BSP_OK)
        {
            status = dev_es8388_init();
        }
        if (status == BSP_OK)
        {
            status = dev_ht6872_init();
        }
        if (status != BSP_OK)
        {
            (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
            return status;
        }
        s_chain_inited = true;
    }

    /* 2. 打开文件 */
    status = bsp_file_open(&s_audio_file, path, BSP_FILE_READ);
    if (status != BSP_OK)
    {
        (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
        log_e("Open %s failed", path);
        return status;
    }

    /* 3. 解析 WAV 头：遍历 RIFF chunk，不硬编码头长 */
    uint8_t hdr[12];
    uint32_t got = 0;
    wav_fmt_t fmt = {0};
    bool fmt_ok = false;
    uint32_t data_size = 0;

    if (bsp_file_read(&s_audio_file, hdr, sizeof(hdr), &got) != BSP_OK || got != sizeof(hdr) ||
        memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)
    {
        log_e("WAV header mismatch: got=%lu [%02X %02X %02X %02X | %02X %02X %02X %02X | %02X %02X %02X %02X]",
              (unsigned long)got, hdr[0], hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6], hdr[7], hdr[8], hdr[9],
              hdr[10], hdr[11]);
        bsp_file_close(&s_audio_file);
        (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
        return BSP_EINVAL;
    }

    for (;;)
    {
        uint8_t chunk_hdr[8];
        if (bsp_file_read(&s_audio_file, chunk_hdr, sizeof(chunk_hdr), &got) != BSP_OK || got != sizeof(chunk_hdr))
        {
            break;      /* 文件尾未找到 data 块 */
        }
        uint32_t chunk_size = (uint32_t)chunk_hdr[4] | ((uint32_t)chunk_hdr[5] << 8) |
                              ((uint32_t)chunk_hdr[6] << 16) | ((uint32_t)chunk_hdr[7] << 24);

        if (memcmp(chunk_hdr, "fmt ", 4) == 0)
        {
            uint8_t fmt_buf[16] = {0};
            uint32_t read_len = (chunk_size < sizeof(fmt_buf)) ? chunk_size : sizeof(fmt_buf);
            if (bsp_file_read(&s_audio_file, fmt_buf, read_len, &got) != BSP_OK || got != read_len)
            {
                break;
            }
            memcpy(&fmt, fmt_buf, sizeof(fmt));
            fmt_ok = true;
            /* 奇数块长按 RIFF 规范补 1 字节对齐 */
            chunk_size = (chunk_size > sizeof(fmt_buf)) ? chunk_size - sizeof(fmt_buf) : 0;
            if (chunk_size & 1U)
            {
                chunk_size++;
            }
            for (uint32_t i = 0; i < chunk_size; i++)
            {
                (void)bsp_file_read(&s_audio_file, hdr, 1, &got);
            }
        }
        else if (memcmp(chunk_hdr, "data", 4) == 0)
        {
            data_size = chunk_size;
            break;
        }
        else
        {
            /* 跳过未知 chunk（LIST 等），奇数长度补齐 */
            uint32_t skip = chunk_size + (chunk_size & 1U);
            while (skip > 0U)
            {
                uint32_t step = (skip > sizeof(hdr)) ? sizeof(hdr) : skip;
                if (bsp_file_read(&s_audio_file, hdr, step, &got) != BSP_OK || got != step)
                {
                    break;
                }
                skip -= step;
            }
        }
    }

    if (!fmt_ok || data_size == 0U || fmt.format != 1U || fmt.bits_per_sample != WAV_BITS_PER_SAMPLE ||
        fmt.sample_rate != WAV_SAMPLE_RATE || (fmt.channels != 1U && fmt.channels != 2U))
    {
        bsp_file_close(&s_audio_file);
        (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
        log_e("Unsupported WAV: rate=%u bits=%u ch=%u", fmt.sample_rate, fmt.bits_per_sample, fmt.channels);
        return BSP_EINVAL;
    }

    /* 4. 预填双缓冲并启动 DMA 流 */
    s_data_remaining = data_size;
    s_channels = (uint8_t)fmt.channels;
    bool eof = audio_fill_half(s_audio_buf[0]);
    eof |= audio_fill_half(s_audio_buf[1]);
    if (eof && s_data_remaining == 0U)
    {
        bsp_file_close(&s_audio_file);
        (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_I2S2);
        return BSP_EINVAL;
    }

    s_half_pending = false;
    s_full_pending = false;
    s_playing = true;
    (void)dev_es8388_start_dac();
    (void)dev_ht6872_enable(true);

    status = port_i2s_start_dma(PORT_I2S_1, s_audio_buf[0], AUDIO_BUF_SAMPLES, audio_half_cb, audio_full_cb, NULL);
    if (status != BSP_OK)
    {
        audio_cleanup();
        return status;
    }

    multiTimerStart(&s_fill_timer, AUDIO_FILL_PERIOD_MS, audio_fill_timer_cb, NULL);
    log_i("Playing %s (%uHz %uch %ubit)", path, fmt.sample_rate, fmt.channels, fmt.bits_per_sample);
    return BSP_OK;
}

bsp_status_t bsp_audio_stop(void)
{
    if (!s_playing)
    {
        return BSP_OK;
    }
    audio_cleanup();
    return BSP_OK;
}

bool bsp_audio_is_playing(void)
{
    return s_playing;
}
