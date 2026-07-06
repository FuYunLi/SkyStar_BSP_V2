/**
 * @file app_ymodem_demo.c
 * @brief Ymodem 文件接收应用演示源文件
 */

#include "app_ymodem_demo.h"
#include "ymodem.h"
#include "bsp_uart.h"
#include "bsp_file.h"
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
static bsp_file_t s_ymodem_file;
static volatile bool s_ymodem_file_opened = false;
static uint32_t s_last_tick = 0U;

/* 存储目标前缀，默认为 0:/ (FatFS/SD卡) */
static char s_filepath_prefix[64] = "0:/";

/* 异步写入错误记录变量，避免在传输过程中打印日志干扰通信 */
static bsp_status_t s_last_write_status = BSP_OK;
static uint32_t s_last_write_bytes = 0;
static uint32_t s_last_write_expected = 0;
static bool s_has_write_error = false;

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
    bsp_status_t status;

    /* 如果之前有文件处于打开状态（例如批量传输中的上一个文件），必须先关闭它 */
    if (s_ymodem_file_opened)
    {
        (void)bsp_file_close(&s_ymodem_file);
        s_ymodem_file_opened = false;
    }

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

    /* 动态拼接存储前缀与纯文件名 */
    snprintf(filepath, sizeof(filepath), "%s%s", s_filepath_prefix, basename);
    // log_i("Ymodem: Creating destination file: %s (%u bytes)", filepath, (unsigned int)filesize);

    /* 递归创建目标目录，防止因路径目录不存在导致打开失败 */
    (void)bsp_file_mkdir_rec(s_filepath_prefix);

    status = bsp_file_open(&s_ymodem_file, filepath, BSP_FILE_CREATE | BSP_FILE_TRUNC | BSP_FILE_WRITE);
    if (status != BSP_OK)
    {
        // log_e("Ymodem: Failed to create file: %s (Error code: %d)", filepath, (int)status);
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
    uint32_t bw = 0;
    bsp_status_t status;

    if (!s_ymodem_file_opened)
    {
        // log_e("Ymodem: Data received but write file is not open!");
        return -1;
    }

    status = bsp_file_write(&s_ymodem_file, data, size, &bw);
    if (status != BSP_OK || bw != size)
    {
        s_last_write_status = status;
        s_last_write_bytes = bw;
        s_last_write_expected = size;
        s_has_write_error = true;
        return -1;
    }

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
        (void)bsp_file_close(&s_ymodem_file);
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
        if (s_has_write_error)
        {
            log_e("Ymodem: Last write error: status=%d, written=%u/%u",
                  (int)s_last_write_status, (unsigned int)s_last_write_bytes, (unsigned int)s_last_write_expected);
            s_has_write_error = false; /* 重置 */
        }
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
    s_last_tick = bsp_tick_get_ms();
    return ymodem_init(&s_ymodem_ctx, &s_ymodem_ops) == 0 ? BSP_OK : BSP_ERROR;
}

/**
 * @brief Ymodem 演示轮询处理任务
 */
void app_ymodem_demo_process(void)
{
    uint32_t current_tick = bsp_tick_get_ms();
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
    if (g_ymodem_active)
    {
        log_w("Ymodem: Session is already active.");
        return -1;
    }

    /* 解析存储目标路径前缀 */
    if (argc >= 2 && strcmp(argv[1], "-flash") == 0)
    {
        strncpy(s_filepath_prefix, "flash/", sizeof(s_filepath_prefix) - 1);
        s_filepath_prefix[sizeof(s_filepath_prefix) - 1] = '\0';
        log_i("Ymodem: Target storage set to Board SPI Flash (LittleFS)");
    }
    else if (argc >= 3 && strcmp(argv[1], "-d") == 0)
    {
        strncpy(s_filepath_prefix, argv[2], sizeof(s_filepath_prefix) - 1);
        s_filepath_prefix[sizeof(s_filepath_prefix) - 1] = '\0';
        
        /* 确保路径以斜杠结尾 */
        uint32_t len = strlen(s_filepath_prefix);
        if (len > 0 && s_filepath_prefix[len - 1] != '/' && s_filepath_prefix[len - 1] != '\\')
        {
            if (len < sizeof(s_filepath_prefix) - 1)
            {
                s_filepath_prefix[len] = '/';
                s_filepath_prefix[len + 1] = '\0';
            }
        }
        log_i("Ymodem: Target storage set to custom path: %s", s_filepath_prefix);
    }
    else
    {
        strncpy(s_filepath_prefix, "0:/", sizeof(s_filepath_prefix) - 1);
        s_filepath_prefix[sizeof(s_filepath_prefix) - 1] = '\0';
        log_i("Ymodem: Target storage set to SD Card (FatFS)");
    }

    log_i("Ymodem: Starting transfer listener...");
    log_i("Ymodem: Please send file via Ymodem protocol from your terminal now.");

    (void)app_ymodem_demo_init();

    /* 启动监听前，清空串口接收环形缓冲区，防止指令本身的回显或换行符干扰状态机 */
    uint8_t dummy;
    while (bsp_uart_read(&dummy, 1U) > 0)
    {
        /* 丢弃残留字符 */
    }

    g_ymodem_active = true;
    s_ymodem_ctx.state = YMODEM_STATE_INIT;
    s_ymodem_ctx.timer_ms = 0U;
    s_ymodem_ctx.init_retry_count = 0U;

    /* 发送初始 'C' 开始接收 */
    s_ymodem_ctx.ops->send_char(&s_ymodem_ctx, YMODEM_C);

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ymodem_recv, shell_ymodem_recv, "Start Ymodem receiver. Usage: ymodem_recv [-flash] [-d <dir>]");
