/**
 * @file app_ota_demo.c
 * @brief OTA 固件升级自检演示实现
 * @note 对标 RocketPi mqtt_ota 传输-落盘环节（UART Ymodem 版，网络
 *       OTA 等 lwIP 接入后仅替换传输层）。调度模型与 app_ymodem_demo
 *       完全同构：会话激活后泵喂字节，ops 回调直通 bsp_ota 三段接口。
 *       流程：`ota_recv` → PC Ymodem 发送 .bin → 自动 commit READY
 *       → 手动复位 → Bootloader 校验拷贝 → 新版本运行。
 *       【未验证】OTA 闭环尚未上板实测。
 */

#define LOG_TAG "APP_OTA"

#include "app_ota_demo.h"
#include "bsp_logger.h"
#include "bsp_ota.h"
#include "bsp_uart.h"
#include "dev_w25q.h"
#include "ota_image.h"
#include "MultiTimer.h"
#include "shell.h"
#include "ymodem.h"
#include <string.h>

/* ================================================================
 * 私有变量
 * ================================================================ */

static ymodem_ctx_t s_ctx;
static bool s_session_active;
static uint32_t s_last_tick;
static uint32_t s_next_offset;
static uint32_t s_bytes;
static uint32_t s_filesize;
static uint32_t s_tx_drop;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 协议侧发送单字节
 */
static void s_ota_send_char(ymodem_ctx_t *ctx, uint8_t ch)
{
    (void)ctx;

    if (bsp_uart_write(&ch, 1U) != BSP_OK)
    {
        s_tx_drop++;
    }
}

/**
 * @brief 文件打开回调：以声明大小开启 OTA 会话（擦区+写 DOWNLOADING 头）
 */
static int s_ota_on_file_open(ymodem_ctx_t *ctx, const char *filename, uint32_t filesize)
{
    (void)ctx;
    (void)filename;

    s_filesize = filesize;

    bsp_status_t ret = bsp_ota_begin(filesize);
    if (ret != BSP_OK)
    {
        log_e("ota begin failed! ret=%d size=%lu", ret, (unsigned long)filesize);
        return -1;
    }

    s_next_offset = 0U;
    s_bytes = 0U;
    return 0;
}

/**
 * @brief 数据写入回调：偏移连续性守卫 + 直通 bsp_ota_write
 */
static int s_ota_on_write(ymodem_ctx_t *ctx, uint32_t offset, const uint8_t *data, uint32_t len)
{
    (void)ctx;

    if (offset != s_next_offset)
    {
        log_e("offset discontinuity: %lu != %lu", (unsigned long)offset,
              (unsigned long)s_next_offset);
        return -1;
    }

    if (bsp_ota_write(data, len) != BSP_OK)
    {
        log_e("ota write failed at %lu", (unsigned long)offset);
        return -1;
    }

    s_next_offset += len;
    s_bytes += len;
    return 0;
}

/**
 * @brief 会话结束回调：传输完整则提交（头部置 READY）
 */
static void s_ota_on_transfer_end(ymodem_ctx_t *ctx, ymodem_result_t result)
{
    (void)ctx;

    if (result == YMODEM_OK && s_bytes == s_filesize)
    {
        if (bsp_ota_commit() == BSP_OK)
        {
            log_i("OTA committed! reboot to apply (bytes=%lu).", (unsigned long)s_bytes);
        }
        return;
    }

    log_e("OTA transfer FAILED result=%d bytes=%lu/%lu, session aborted.",
          (int)result, (unsigned long)s_bytes, (unsigned long)s_filesize);
    (void)bsp_ota_abort();
}

/**
 * @brief 回调配置体
 */
static const ymodem_ops_t s_ota_ops =
{
    .send_char       = s_ota_send_char,
    .on_file_open    = s_ota_on_file_open,
    .on_write        = s_ota_on_write,
    .on_transfer_end = s_ota_on_transfer_end
};

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 OTA 演示模块
 */
bsp_status_t app_ota_demo_init(void)
{
    s_session_active = false;
    s_last_tick = bsp_tick_get_ms();

    if (ymodem_init(&s_ctx, YMODEM_MODE_RECEIVER, &s_ota_ops) != 0)
    {
        return BSP_EINVAL;
    }

    log_i("OTA Demo loaded. ver=0x%08lX. Try: ota_recv", (unsigned long)OTA_APP_VERSION);
    return BSP_OK;
}

/**
 * @brief OTA 接收泵
 */
void app_ota_demo_process(void)
{
    uint32_t current_tick = bsp_tick_get_ms();
    uint32_t elapsed = current_tick - s_last_tick;
    s_last_tick = current_tick;

    if (!s_session_active)
    {
        return;
    }

    ymodem_tick(&s_ctx, elapsed);

    uint8_t rx_byte;
    while (bsp_uart_read(&rx_byte, 1U) > 0U)
    {
        ymodem_receive_byte(&s_ctx, rx_byte);
    }

    ymodem_state_t st = ymodem_get_state(&s_ctx);
    if (st == YMODEM_STATE_IDLE || st == YMODEM_STATE_DONE || st == YMODEM_STATE_ERROR)
    {
        uint8_t tail;
        while (bsp_uart_read(&tail, 1U) > 0U)
        {
        }
        s_session_active = false;
    }
}

/* ================================================================
 * Shell 命令注册
 * ================================================================ */

/**
 * @brief ota_recv Shell 命令入口：启动 OTA 接收会话
 */
static int shell_ota_recv(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (s_session_active)
    {
        log_w("OTA session already active.");
        return -1;
    }

    if (ymodem_init(&s_ctx, YMODEM_MODE_RECEIVER, &s_ota_ops) != 0)
    {
        log_e("ymodem init failed (ops incomplete)");
        return -1;
    }

    /* 排空残留字节（命令回显/换行），避免噪声喂进状态机 */
    uint8_t dummy;
    while (bsp_uart_read(&dummy, 1U) > 0U)
    {
    }

    s_last_tick = bsp_tick_get_ms();
    s_next_offset = 0U;
    s_bytes = 0U;
    s_tx_drop = 0U;
    s_session_active = true;
    ymodem_start(&s_ctx);

    log_i("OTA session started, waiting for sender...");
    return 0;
}

/**
 * @brief ota_state Shell 命令入口：读取并打印 W25Q 头部原始状态
 */
static int shell_ota_state(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    ota_header_t hdr;
    bsp_status_t ret = dev_w25q_read(OTA_HEADER_ADDR, (uint8_t *)&hdr, sizeof(hdr));
    if (ret != BSP_OK)
    {
        log_e("read header failed! ret=%d", ret);
        return -1;
    }

    log_i("magic=%08lX size=%lu crc=%08lX ver=%08lX state=%lu",
          (unsigned long)hdr.magic, (unsigned long)hdr.image_size,
          (unsigned long)hdr.image_crc, (unsigned long)hdr.version,
          (unsigned long)hdr.state);
    return 0;
}

/**
 * @brief ver Shell 命令入口：打印当前 App 版本（OTA 闭环的验收判据）
 */
static int shell_ver(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    log_i("App version = 0x%08lX (v%lu.%lu.%lu)",
          (unsigned long)OTA_APP_VERSION,
          (unsigned long)((OTA_APP_VERSION >> 16) & 0xFFU),
          (unsigned long)((OTA_APP_VERSION >> 8) & 0xFFU),
          (unsigned long)(OTA_APP_VERSION & 0xFFU));
    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ota_recv, shell_ota_recv, Start OTA firmware receive session);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ota_state, shell_ota_state, Dump OTA header state from W25Q);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ver, shell_ver, Print App firmware version);
