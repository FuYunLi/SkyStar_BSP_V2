/**
 * @file app_ymodem_demo.c
 * @brief Ymodem 文件接收应用演示源文件
 */

#include "app_ymodem_demo.h"
#include "ymodem.h"
#include "bsp_uart.h"
#include "fatfs.h"
#include "shell.h"

#define LOG_TAG "YMODEM_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * 全局与静态变量
 * ================================================================ */
volatile bool g_ymodem_active = false;

static ymodem_ctx_t s_ymodem_ctx;
static FIL s_ymodem_file;
static volatile bool s_ymodem_file_opened = false;
static uint32_t s_last_tick = 0U;

/* ================================================================
 * Ymodem 底层操作回调函数实现
 * ================================================================ */

/**
 * @brief 发送字符回调
 */
static void s_ymodem_send_char(ymodem_ctx_t *ctx, uint8_t ch)
{
    (void)ctx;
    (void)bsp_uart_write(&ch, 1U);
}

/**
 * @brief 解析到文件头回调
 */
static int s_ymodem_on_file_header(ymodem_ctx_t *ctx, const char *filename, uint32_t filesize)
{
    (void)ctx;
    char filepath[128];
    FRESULT fr;

    /* 剥离可能含有的目录前缀，仅提取纯文件名 */
    const char *basename = filename;
    const char *p = filename;
    while (*p != '\0')
    {
        if (*p == '/' || *p == '\\')
        {
            basename = p + 1;
        }
        p++;
    }

    /* 拼接写入 TF 卡的完整路径 */
    snprintf(filepath, sizeof(filepath), "0:/%s", basename);
    log_i("Ymodem: Creating destination file: %s (%u bytes)", filepath, (unsigned int)filesize);

    fr = f_open(&s_ymodem_file, filepath, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        log_e("Ymodem: Failed to create file: %s (Error code: %d)", filepath, (int)fr);
        s_ymodem_file_opened = false;
        return -1;
    }

    s_ymodem_file_opened = true;
    return 0;
}

/**
 * @brief 数据块接收回调
 */
static int s_ymodem_on_data_block(ymodem_ctx_t *ctx, const uint8_t *data, uint32_t offset, uint32_t size)
{
    (void)ctx;
    (void)offset;
    UINT bw = 0;
    FRESULT fr;

    if (!s_ymodem_file_opened)
    {
        log_e("Ymodem: Data received but write file is not open!");
        return -1;
    }

    fr = f_write(&s_ymodem_file, data, size, &bw);
    if (fr != FR_OK || bw != size)
    {
        log_e("Ymodem: File write error: %d (Written: %u/%u)", (int)fr, (unsigned int)bw, (unsigned int)size);
        return -1;
    }

    /* 实时数据刷新，以防传输突发中断导致数据损坏 */
    (void)f_sync(&s_ymodem_file);
    
    return 0;
}

/**
 * @brief 传输结束回调
 */
static void s_ymodem_on_transfer_end(ymodem_ctx_t *ctx, ymodem_result_t result)
{
    (void)ctx;
    
    if (s_ymodem_file_opened)
    {
        (void)f_close(&s_ymodem_file);
        s_ymodem_file_opened = false;
    }

    g_ymodem_active = false;

    if (result == YMODEM_OK)
    {
        log_i("Ymodem: File received successfully.");
    }
    else
    {
        log_e("Ymodem: File transfer failed or aborted (Code: %d).", (int)result);
    }
}

/* ================================================================
 * 回调配置接口体
 * ================================================================ */
static const ymodem_ops_t s_ymodem_ops =
{
    .send_char = s_ymodem_send_char,
    .on_file_header = s_ymodem_on_file_header,
    .on_data_block = s_ymodem_on_data_block,
    .on_transfer_end = s_ymodem_on_transfer_end
};

/* ================================================================
 * 应用接口层实现
 * ================================================================ */

/**
 * @brief 初始化 Ymodem 演示应用
 */
bsp_status_t app_ymodem_demo_init(void)
{
    g_ymodem_active = false;
    s_last_tick = HAL_GetTick();
    return ymodem_init(&s_ymodem_ctx, &s_ymodem_ops) == 0 ? BSP_OK : BSP_ERROR;
}

/**
 * @brief Ymodem 演示轮询处理任务
 */
void app_ymodem_demo_process(void)
{
    uint32_t current_tick = HAL_GetTick();
    uint32_t elapsed = current_tick - s_last_tick;
    s_last_tick = current_tick;

    if (!g_ymodem_active)
    {
        return;
    }

    /* 1. 超时及握手重试轮询 */
    ymodem_tick(&s_ymodem_ctx, elapsed);

    /* 2. 非阻塞读取串口缓冲并喂状态机 */
    uint8_t rx_byte;
    while (bsp_uart_read(&rx_byte, 1U) > 0)
    {
        ymodem_receive_byte(&s_ymodem_ctx, rx_byte);
    }
}

/* ================================================================
 * Shell 命令注册
 * ================================================================ */

/**
 * @brief ymodem_recv Shell 命令处理函数
 */
static int shell_ymodem_recv(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (g_ymodem_active)
    {
        log_w("Ymodem: Session is already active.");
        return -1;
    }

    log_i("Ymodem: Starting transfer listener...");
    log_i("Ymodem: Please send file via Ymodem protocol from your terminal now.");

    (void)app_ymodem_demo_init();

    g_ymodem_active = true;
    s_ymodem_ctx.state = YMODEM_STATE_INIT;
    s_ymodem_ctx.timer_ms = 0U;
    s_ymodem_ctx.init_retry_count = 0U;

    /* 发送初始 'C' 开始接收 */
    s_ymodem_ctx.ops->send_char(&s_ymodem_ctx, YMODEM_C);

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ymodem_recv, shell_ymodem_recv, "Start Ymodem receiver");
