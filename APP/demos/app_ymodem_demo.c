/**
 * @file app_ymodem_demo.c
 * @brief Ymodem 文件接收应用演示源文件
 */

#include "app_ymodem_demo.h"
#include "ymodem.h"
#include "bsp_uart.h"
#include "bsp_file.h"
#include "shell.h"
#include <string.h>

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

/* 异步记录区：会话期间绝对禁止打印日志，详见 s_ymodem_on_transfer_end 注释
 * （发送端 wait_for_char('C') 逐字节扫 0x43，日志文本里的大写 C 会造成伪握手） */
static char s_ymodem_filename[128];               /* 本会话最后一个文件名，会话结束后才打印 */
static uint32_t s_ymodem_next_offset = 0U;        /* 期望写入偏移，用于连续性守卫 */
static uint32_t s_ymodem_bytes = 0U;              /* 本会话累计已写入字节 */
static uint32_t s_ymodem_filesize = 0U;
static bsp_status_t s_ymodem_last_status = BSP_OK; /* 最后一次失败的底层状态码 */
static bool s_ymodem_close_failed = false;
static uint32_t s_ymodem_tx_drop = 0U;            /* 发送队列满导致丢弃的协议字节数 */

/* ================================================================
 * Ymodem 底层操作回调函数实现
 * ================================================================ */

/**
 * @brief 发送字符回调
 * @note  队列式发送，入队即返回，符合 V3 “必须尽快返回”约束；
 *        队列满时丢一个协议字节的后果由对端超时重传兜底，但必须计数上报，
 *        绝不静默吞掉
 */
static void s_ymodem_send_char(ymodem_ctx_t *ctx, uint8_t ch)
{
    (void)ctx;

    if (bsp_uart_write(&ch, 1U) != BSP_OK)
    {
        s_ymodem_tx_drop++;
    }
}

/**
 * @brief 解析到文件头回调（V3 名：on_file_open）
 * @note  会话期间不打日志，只记录名称与长度，留待会话结束时一次性报告
 */
static int s_ymodem_on_file_open(ymodem_ctx_t *ctx, const char *filename, uint32_t filesize)
{
    char filepath[160];
    bsp_status_t status;

    (void)ctx;

    /* 如果之前有文件处于打开状态（批量传输中上一个文件的异常兜底），先关闭它 */
    if (s_ymodem_file_opened)
    {
        if (bsp_file_close(&s_ymodem_file) != BSP_OK)
        {
            s_ymodem_close_failed = true;
        }
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
    snprintf(s_ymodem_filename, sizeof(s_ymodem_filename), "%s", basename);
    s_ymodem_filesize = filesize;

    /* 递归创建目标目录，防止因路径目录不存在导致打开失败 */
    (void)bsp_file_mkdir_rec(s_filepath_prefix);

    status = bsp_file_open(&s_ymodem_file, filepath, BSP_FILE_CREATE | BSP_FILE_TRUNC | BSP_FILE_WRITE);
    if (status != BSP_OK)
    {
        s_ymodem_last_status = status;
        s_ymodem_file_opened = false;
        return -1;
    }

    s_ymodem_file_opened = true;
    s_ymodem_next_offset = 0U;
    s_ymodem_bytes = 0U;
    return 0;
}

/* 落盘中转缓冲：以 uint32_t 为底保证 4 字节对齐，容量等于最大载荷 1024 字节 */
#define YMODEM_STAGE_SIZE (1024U)
static uint32_t s_ymodem_stage[YMODEM_STAGE_SIZE / 4U];

/**
 * @brief 数据块写入回调（V3 名：on_write，注意参数顺序为 offset 在前）
 * @note  两道防线：
 *       1) 偏移连续性守卫：协议错位/重写当场失败上报，不事后靠 CRC 对账发现；
 *       2) 非 4 字节对齐源经对齐中转缓冲再落盘：FatFS 对整扇区写是把用户指针直达
 *          SDIO IDMA(ff.c direct-write)，而 IDMA 会丢弃地址低 2 位，导致整块位移
 *          ——V3 的 ctx 布局下 frame_buf+3 恰为 4n+3，必现静默数据损坏
 */
static int s_ymodem_on_write(ymodem_ctx_t *ctx, uint32_t offset, const uint8_t *data, uint32_t len)
{
    uint32_t bw = 0;
    bsp_status_t status;

    (void)ctx;

    if (!s_ymodem_file_opened)
    {
        s_ymodem_last_status = BSP_EINVAL;
        return -1;
    }

    if (offset != s_ymodem_next_offset)
    {
        s_ymodem_last_status = BSP_EINVAL;
        return -1;
    }

    if (len > YMODEM_STAGE_SIZE)
    {
        s_ymodem_last_status = BSP_EINVAL;
        return -1;
    }

    if (((uint32_t)(uintptr_t)data & 0x3U) != 0U)
    {
        memcpy(s_ymodem_stage, data, len);
        data = (const uint8_t *)s_ymodem_stage;
    }

    status = bsp_file_write(&s_ymodem_file, data, len, &bw);
    if (status != BSP_OK || bw != len)
    {
        s_ymodem_last_status = status;
        return -1;
    }

    s_ymodem_next_offset += len;
    s_ymodem_bytes += len;
    return 0;
}

/**
 * @brief 单文件结束回调（V3 新增）：真正的落盘时刻在这里，不得吞掉 close 失败
 */
static void s_ymodem_on_file_close(ymodem_ctx_t *ctx, ymodem_result_t result)
{
    (void)ctx;
    (void)result;

    if (!s_ymodem_file_opened)
    {
        return;
    }

    if (bsp_file_close(&s_ymodem_file) != BSP_OK)
    {
        s_ymodem_close_failed = true;
    }
    s_ymodem_file_opened = false;
}

/**
 * @brief 会话结束回调：一次性报告全部结果
 * @note  之所以把日志集中在会话结束才打：传输期间 ACK/NAK/'C' 与日志共用同一条
 *       TX 队列，对端逐字节扫 'C'，日志里的字母会干扰握手
 */
static void s_ymodem_on_transfer_end(ymodem_ctx_t *ctx, ymodem_result_t result)
{
    (void)ctx;

    if (s_ymodem_file_opened)
    {
        if (bsp_file_close(&s_ymodem_file) != BSP_OK)
        {
            s_ymodem_close_failed = true;
        }
        s_ymodem_file_opened = false;
    }

    g_ymodem_active = false;

    if (result == YMODEM_OK && !s_ymodem_close_failed && s_ymodem_bytes == s_ymodem_filesize)
    {
        log_i("Ymodem: OK  file=%s  bytes=%lu/%lu  tx_drop=%lu",
              s_ymodem_filename, (unsigned long)s_ymodem_bytes, (unsigned long)s_ymodem_filesize,
              (unsigned long)s_ymodem_tx_drop);
    }
    else
    {
        log_e("Ymodem: FAILED result=%d  bytes=%lu/%lu  close_failed=%d  last_status=%d  tx_drop=%lu",
              (int)result, (unsigned long)s_ymodem_bytes, (unsigned long)s_ymodem_filesize,
              (int)s_ymodem_close_failed, (int)s_ymodem_last_status, (unsigned long)s_ymodem_tx_drop);
    }
}

/* ================================================================
 * 回调配置接口体
 * ================================================================ */
static const ymodem_ops_t s_ymodem_ops =
{
    .send_char      = s_ymodem_send_char,
    .on_file_open   = s_ymodem_on_file_open,
    .on_write       = s_ymodem_on_write,
    .on_file_close  = s_ymodem_on_file_close,
    .on_transfer_end = s_ymodem_on_transfer_end
};

/* ================================================================
 * 应用接口层实现
 * ================================================================ */

/**
 * @brief 初始化 Ymodem 演示应用（仅初始化上下文，不启动会话）
 */
bsp_status_t app_ymodem_demo_init(void)
{
    g_ymodem_active = false;
    s_last_tick = bsp_tick_get_ms();

    if (ymodem_init(&s_ymodem_ctx, YMODEM_MODE_RECEIVER, &s_ymodem_ops) != 0)
    {
        return BSP_EINVAL;
    }
    return BSP_OK;
}

/**
 * @brief 启动一次接收会话：清会话计数、排空旧字节、交给协议栈自己发首个 'C'
 * @return bsp_status_t BSP_OK 已启动；BSP_ERROR 上下文未初始化
 */
static bsp_status_t s_ymodem_begin(void)
{
    s_ymodem_filename[0] = '\0';
    s_ymodem_next_offset = 0U;
    s_ymodem_bytes = 0U;
    s_ymodem_filesize = 0U;
    s_ymodem_last_status = BSP_OK;
    s_ymodem_close_failed = false;
    s_ymodem_tx_drop = 0U;

    if (ymodem_init(&s_ymodem_ctx, YMODEM_MODE_RECEIVER, &s_ymodem_ops) != 0)
    {
        log_e("Ymodem: init failed (ops incomplete)");
        return BSP_ERROR;
    }

    /* 先排空残留字节（上一条命令的回显/换行），再启动，避免噪声喂进状态机 */
    uint8_t dummy;
    while (bsp_uart_read(&dummy, 1U) > 0U)
    {
        /* 丢弃 */
    }

    s_last_tick = bsp_tick_get_ms();
    g_ymodem_active = true;
    ymodem_start(&s_ymodem_ctx);
    return BSP_OK;
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

    /* 3. 会话结束后排空残留字节，并把串口还给 shell
     *    （否则尾随字节会被 shell 误当作命令执行） */
    ymodem_state_t st = ymodem_get_state(&s_ymodem_ctx);
    if (st == YMODEM_STATE_IDLE || st == YMODEM_STATE_DONE || st == YMODEM_STATE_ERROR)
    {
        uint8_t tail;
        while (bsp_uart_read(&tail, 1U) > 0U)
        {
            /* 丢弃会话尾部残留 */
        }
        g_ymodem_active = false;
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
    else if (argc >= 2 && strcmp(argv[1], "-d") == 0)
    {
        if (argc < 3)
        {
            log_w("Ymodem: -d needs a directory argument");
            return -1;
        }

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

    return (s_ymodem_begin() == BSP_OK) ? 0 : -1;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ymodem_recv, shell_ymodem_recv, "Start Ymodem receiver. Usage: ymodem_recv [-flash] [-d <dir>]");
