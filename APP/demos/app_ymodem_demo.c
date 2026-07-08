/**
 * @file app_ymodem_demo.c
 * @brief Ymodem 文件接收应用演示源文件
 */

#include "app_ymodem_demo.h"
#include "ymodem.h"
#include "bsp_uart.h"
#include "bsp_file.h"
#include "shell.h"
#include "fatfs.h"
#include "port_sdio.h"

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

/*
 * 延迟创建文件路径缓冲区：
 * on_file_header 仅记录路径，不执行 f_open（避免 SD 卡 I/O 阻塞 ACK 响应时序）。
 * on_data_block 首次调用（offset == 0）时再执行真正的 f_open。
 */
static char s_pending_filepath[128] = {0};
static bool s_file_open_pending = false;

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
    (void)filesize;

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

    /*
     * [关键优化] 仅记录目标路径，不在此处执行 f_open：
     *
     * 问题：此回调是从 ymodem_receive_byte() 状态机内部同步调用的，
     *       调用完成后状态机才发送 ACK + 'C'。
     *       SD 卡的 f_open 需要 SD 总线 I/O（50-500ms），会导致 ACK 延迟，
     *       超过 PC 端的 ACK 等待超时（约 1-2s），引发重传或失败。
     *       LittleFS（SPI Flash）因为速度快所以不受影响。
     *
     * 修复：此处只做纯内存操作（字符串拼接），实际 f_open 推迟到
     *       on_data_block 首次调用时执行（此时 PC 已进入发包模式，
     *       有充足的包间隔时间等待 ACK）。
     */
    snprintf(s_pending_filepath, sizeof(s_pending_filepath), "%s%s", s_filepath_prefix, basename);
    s_file_open_pending = true;

    return 0;
}

/**
 * @brief 数据块接收回调
 */
static int s_ymodem_on_data_block(ymodem_ctx_t *ctx, const uint8_t *data, uint32_t offset, uint32_t size)
{
    (void)ctx;
    uint32_t bw = 0;
    bsp_status_t status;

    /* 首块到达时才执行真正的文件创建（包含 SD 卡 I/O），此时 ACK 已发出，时序安全 */
    if (s_file_open_pending && offset == 0U)
    {
        s_file_open_pending = false;

        /* 递归创建目标目录，防止因路径目录不存在导致打开失败 */
        (void)bsp_file_mkdir_rec(s_filepath_prefix);

        status = bsp_file_open(&s_ymodem_file, s_pending_filepath,
                               BSP_FILE_CREATE | BSP_FILE_TRUNC | BSP_FILE_WRITE);
        if (status != BSP_OK)
        {
            s_ymodem_file_opened = false;
            return -1;
        }
        s_ymodem_file_opened = true;
    }

    if (!s_ymodem_file_opened)
    {
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

    /* 重置延迟文件创建状态，防止上次会话残留标志干扰新会话 */
    s_file_open_pending = false;
    s_pending_filepath[0] = '\0';
    s_has_write_error = false;

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

    /* ----------------------------------------------------------------
     * [SD 卡预飞检查]
     * 对于 FatFS (0:/) 路径，在协议启动前验证文件系统已挂载。
     * 使用 SDFatFS.fs_type 判断：0 = 未挂载，非 0 = 已挂载。
     * 注意：不可用 f_stat("0:/") 检查根目录，FatFS 会返回 FR_INVALID_NAME。
     * ---------------------------------------------------------------- */
    if (strncmp(s_filepath_prefix, "0:", 2) == 0)
    {
        if (SDFatFS.fs_type == 0)
        {
            log_w("Ymodem: FatFS not mounted, attempting auto-mount...");

            bsp_status_t sdio_st = port_sdio_init();
            if (sdio_st != BSP_OK)
            {
                log_e("Ymodem: SD card HW init failed (%d). Abort.", (int)sdio_st);
                return -1;
            }

            FRESULT fr = f_mount(&SDFatFS, (const TCHAR *)SDPath, 1);
            if (fr != FR_OK)
            {
                log_e("Ymodem: FatFS mount failed (fr=%d). Abort.", (int)fr);
                return -1;
            }
            log_i("Ymodem: FatFS auto-mounted OK.");
        }
        else
        {
            log_i("Ymodem: FatFS already mounted (fs_type=%d).", (int)SDFatFS.fs_type);
        }
    }

    log_i("Ymodem: Starting transfer listener...");
    log_i("Ymodem: Please send file via Ymodem protocol from your terminal now.");

    /*
     * [关键时序修复]
     * 问题：log_i 是异步发送（写入 TX LwRB 后立即返回），
     *       如果在日志字节未物理发出前就发 'C'，PC 收到 'C' 会立即发 Packet 0，
     *       而此时 MCU UART 还在忙于发日志，导致 Packet 0 无法被正确接收，
     *       帧错误后 MCU 状态机回应 CAN，传输终止。
     *
     * Letter-Shell 在本命令函数返回后还会自动打印 "letter:/$" 提示符，
     * 若该提示符的发送时间晚于 'C'，则 PC 已在传输态，提示符字节会成为协议噪声。
     *
     * 修复策略：
     *   Step 1: 等待当前 TX 队列（含日志）排空
     *   Step 2: 延迟 50ms，给 Shell 框架时间回显提示符并写入 TX 队列
     *   Step 3: 再次等待 TX 队列排空（确保提示符也已物理发出）
     *   Step 4: 清空 RX 缓冲区（此时 UART 线路已静默）
     *   Step 5: 发出 'C' 开始握手
     */

    /* Step 1: 等待 log_i 日志彻底从 TX 队列发出（最多 500ms） */
    (void)bsp_uart_wait_tx_done(500U);

    /* Step 2: 忙等 50ms，确保 Letter-Shell 命令返回后打印提示符并进入 TX 队列 */
    {
        uint32_t wait_start = bsp_tick_get_ms();
        while ((bsp_tick_get_ms() - wait_start) < 50U)
        {
            /* 忙等 */
        }
    }

    /* Step 3: 等待 Shell 提示符也物理发出（最多 200ms） */
    (void)bsp_uart_wait_tx_done(200U);

    (void)app_ymodem_demo_init();

    /* Step 4: 清空 RX 环形缓冲区，丢弃传输前可能残留的任何字节 */
    uint8_t dummy;
    while (bsp_uart_read(&dummy, 1U) > 0)
    {
        /* 丢弃残留字符 */
    }

    g_ymodem_active = true;
    s_ymodem_ctx.state = YMODEM_STATE_INIT;
    s_ymodem_ctx.timer_ms = 0U;
    s_ymodem_ctx.init_retry_count = 0U;

    /* Step 5: 发送初始 'C' 开始接收 */
    s_ymodem_ctx.ops->send_char(&s_ymodem_ctx, YMODEM_C);

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ymodem_recv, shell_ymodem_recv, "Start Ymodem receiver. Usage: ymodem_recv [-flash] [-d <dir>]");
