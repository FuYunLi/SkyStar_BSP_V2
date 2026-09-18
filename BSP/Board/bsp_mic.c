/**
 * @file bsp_mic.c
 * @brief 板级麦克风录音服务层实现
 * @note 调度模型与 bsp_audio 对称：DMA 收满一块置交换标志（ISR
 *       最小化），主循环泵把数据写进 FatFS 后重新挂接收 DMA。
 *       录音数据为立体声交错 16 位（ES8388 左通道为板载差分麦克风，
 *       右通道默认静音，WAV 头按双声道声明）。
 *       【未验证】录音链路尚未上板实测。
 */

#define LOG_TAG "BSP_MIC"

#include "bsp_mic.h"
#include "bsp_logger.h"
#include "bsp_file.h"
#include "dev_es8388.h"
#include "port_i2s.h"
#include <string.h>

/* ================================================================
 * 私有宏定义与常量
 * ================================================================ */

#define MIC_BUF_SAMPLES     (2048U)  /* 单缓冲采样元素数（4KB） */
#define MIC_DEFAULT_RATE    (16000U)
#define MIC_RATE_MIN        (8000U)
#define MIC_RATE_MAX        (48000U)
#define MIC_SECONDS_MAX     (60U)

/* ================================================================
 * 私有变量
 * ================================================================ */

static uint16_t s_mic_buf[2][MIC_BUF_SAMPLES]; /* 双缓冲，常规 SRAM DMA 可达 */
static uint16_t s_buf_samples[2];              /* 各缓冲有效采样数，0 为结束标记 */

static bsp_file_t s_mic_file;
static bool s_file_open;
static bool s_recording;
static uint8_t s_active_buf;                   /* DMA 正在接收的缓冲下标 */
static uint32_t s_samples_target;              /* 目标总采样元素数 */
static uint32_t s_samples_captured;            /* 已落盘采样元素数 */
static volatile bool s_swap_request;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 写 WAV 文件头（44 字节标准头，数据尺寸一次写全）
 */
static bsp_status_t s_wav_write_header(uint32_t sample_rate, uint32_t data_bytes)
{
    uint32_t file_size = data_bytes + 36U;
    uint32_t byte_rate = sample_rate * 2U * 2U; /* 双声道 × 16 位 */
    uint8_t hdr[44] = {0};

    hdr[0] = 'R'; hdr[1] = 'I'; hdr[2] = 'F'; hdr[3] = 'F';
    hdr[4] = (uint8_t)(file_size);
    hdr[5] = (uint8_t)(file_size >> 8);
    hdr[6] = (uint8_t)(file_size >> 16);
    hdr[7] = (uint8_t)(file_size >> 24);
    hdr[8] = 'W'; hdr[9] = 'A'; hdr[10] = 'V'; hdr[11] = 'E';
    hdr[12] = 'f'; hdr[13] = 'm'; hdr[14] = 't'; hdr[15] = ' ';
    hdr[16] = 16U; /* fmt 块长 */
    hdr[20] = 1U;  /* PCM */
    hdr[22] = 2U;  /* 双声道 */
    hdr[24] = (uint8_t)(sample_rate);
    hdr[25] = (uint8_t)(sample_rate >> 8);
    hdr[26] = (uint8_t)(sample_rate >> 16);
    hdr[27] = (uint8_t)(sample_rate >> 24);
    hdr[28] = (uint8_t)(byte_rate);
    hdr[29] = (uint8_t)(byte_rate >> 8);
    hdr[30] = (uint8_t)(byte_rate >> 16);
    hdr[31] = (uint8_t)(byte_rate >> 24);
    hdr[32] = 4U;  /* 块对齐（双声道 16 位） */
    hdr[34] = 16U; /* 位宽 */
    hdr[36] = 'd'; hdr[37] = 'a'; hdr[38] = 't'; hdr[39] = 'a';
    hdr[40] = (uint8_t)(data_bytes);
    hdr[41] = (uint8_t)(data_bytes >> 8);
    hdr[42] = (uint8_t)(data_bytes >> 16);
    hdr[43] = (uint8_t)(data_bytes >> 24);

    uint32_t written = 0U;

    return bsp_file_write(&s_mic_file, hdr, sizeof(hdr), &written);
}

/**
 * @brief I2S 接收完成回调（ISR 上下文）：仅置交换标志
 */
static void s_mic_rx_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx)
{
    (void)bus_id;
    (void)result;
    (void)user_ctx;

    if (s_recording)
    {
        s_swap_request = true;
    }
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 开始录音
 */
bsp_status_t bsp_mic_record(const char *path, uint32_t seconds, uint32_t sample_rate)
{
    if ((path == NULL) || (seconds == 0U) || (seconds > MIC_SECONDS_MAX))
    {
        return BSP_EINVAL;
    }

    if (s_recording)
    {
        return BSP_BUSY;
    }

    if ((sample_rate < MIC_RATE_MIN) || (sample_rate > MIC_RATE_MAX))
    {
        return BSP_EINVAL;
    }

    /* 数据尺寸在启动时确定：时长 × 采样率 × 双声道 × 16 位 */
    uint32_t data_bytes = seconds * sample_rate * 2U * 2U;
    s_samples_target = data_bytes / 2U;

    /* 1. 建立录音通道 */
    bsp_status_t ret = port_i2s_init_rx(PORT_I2S_2, sample_rate);
    if (ret != BSP_OK)
    {
        log_e("I2S rx init failed! ret = %d", ret);
        return ret;
    }

    ret = dev_es8388_start_adc();
    if (ret != BSP_OK)
    {
        log_e("ES8388 adc start failed! ret = %d", ret);
        (void)port_i2s_deinit(PORT_I2S_2);
        return ret;
    }

    /* 2. 建文件并写 WAV 头（尺寸已知，无需回填） */
    s_mic_file.type = BSP_FILE_TYPE_UNKNOWN;
    ret = bsp_file_open(&s_mic_file, path, BSP_FILE_WRITE | BSP_FILE_CREATE | BSP_FILE_TRUNC);
    if (ret != BSP_OK)
    {
        log_e("create %s failed! ret = %d", path, ret);
        (void)port_i2s_deinit(PORT_I2S_2);
        (void)dev_es8388_stop_adc();
        return BSP_ERROR;
    }
    s_file_open = true;

    ret = s_wav_write_header(sample_rate, data_bytes);
    if (ret != BSP_OK)
    {
        log_e("write header failed!");
        (void)bsp_file_close(&s_mic_file);
        s_file_open = false;
        (void)port_i2s_deinit(PORT_I2S_2);
        (void)dev_es8388_stop_adc();
        return ret;
    }

    /* 3. 挂第一块接收 DMA */
    s_active_buf = 0U;
    s_buf_samples[0] = (uint16_t)MIC_BUF_SAMPLES;
    s_buf_samples[1] = 0U;
    s_samples_captured = 0U;
    s_swap_request = false;
    s_recording = true;

    ret = port_i2s_read_dma(PORT_I2S_2, s_mic_buf[0], MIC_BUF_SAMPLES, s_mic_rx_cb, NULL);
    if (ret != BSP_OK)
    {
        s_recording = false;
        (void)bsp_file_close(&s_mic_file);
        s_file_open = false;
        (void)port_i2s_deinit(PORT_I2S_2);
        (void)dev_es8388_stop_adc();
        return ret;
    }

    log_i("recording %s (%lus @ %luHz)...", path, (unsigned long)seconds, (unsigned long)sample_rate);
    return BSP_OK;
}

/**
 * @brief 停止录音
 */
bsp_status_t bsp_mic_stop(void)
{
    if (!s_recording)
    {
        return BSP_OK;
    }

    s_recording = false;
    s_swap_request = false;

    (void)port_i2s_stop(PORT_I2S_2);
    (void)dev_es8388_stop_adc();
    (void)port_i2s_deinit(PORT_I2S_2);

    if (s_file_open)
    {
        (void)bsp_file_close(&s_mic_file);
        s_file_open = false;
    }

    log_i("recording stopped, SPI2 restored.");
    return BSP_OK;
}

/**
 * @brief 录音泵：落盘收满的缓冲并续挂接收
 */
void bsp_mic_process(void)
{
    if (!s_recording || !s_swap_request)
    {
        return;
    }

    s_swap_request = false;

    /* 收满的缓冲落盘 */
    uint8_t finished = s_active_buf;
    uint32_t bytes = (uint32_t)s_buf_samples[finished] * 2U;
    uint32_t written = 0U;

    if (bsp_file_write(&s_mic_file, s_mic_buf[finished], bytes, &written) != BSP_OK)
    {
        log_e("write audio data failed!");
        (void)bsp_mic_stop();
        return;
    }

    s_samples_captured += s_buf_samples[finished];

    /* 达到目标时长：收尾 */
    if (s_samples_captured >= s_samples_target)
    {
        (void)bsp_mic_stop();
        log_i("recording finished (%lu samples).", (unsigned long)s_samples_captured);
        return;
    }

    /* 续挂接收 DMA（写入文件期间数据由 ES8388 侧 FIFO 缓冲，
     * FatFS 写延迟会造成丢样，属原型限制；消除需环形缓冲 +
     * 半满中断，列为扩展项） */
    uint8_t next = (uint8_t)(finished ^ 1U);
    bsp_status_t ret = port_i2s_read_dma(PORT_I2S_2, s_mic_buf[next], MIC_BUF_SAMPLES, s_mic_rx_cb, NULL);
    if (ret != BSP_OK)
    {
        log_e("dma rearm failed! ret = %d", ret);
        (void)bsp_mic_stop();
        return;
    }

    s_active_buf = next;
    s_buf_samples[next] = (uint16_t)MIC_BUF_SAMPLES;
}
