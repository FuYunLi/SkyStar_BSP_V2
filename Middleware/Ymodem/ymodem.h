/**
 * @file    ymodem.h
 * @brief   纯 C、平台无关、非阻塞的 Ymodem(YMODEM-CRC)协议中间件 (V3)
 *
 * @details
 *  - 事件驱动状态机：上层通过 ymodem_receive_byte() 逐字节喂入接收数据，
 *    通过 ymodem_tick() 周期驱动超时/重试，中间件内部绝不做阻塞或忙等。
 *  - 同时支持“接收端(receiver)”与“发送端(sender)”两种角色。
 *  - 底层字节收发、文件读写全部通过回调(ymodem_ops_t)与上层解耦。
 *  - 校验方式为 Ymodem 规范 CRC：poly 0x1021、初值 0x0000、MSB-first，
 *    即 CRC-16/XMODEM(注意不是 init=0xFFFF 的 CCITT-FALSE 变体)。
 *  - 帧序号按模 256 回绕，支持超过 255 帧的文件：接收端以“状态”而非
 *    “seq==0”区分文件头与回绕到 0 的数据帧(V1 的致命缺陷)。
 *  - 发送端数据帧固定使用 STX(1024 字节)，对端须支持 YMODEM-CRC 的
 *    1024 字节帧(Ymodem 标准接收端均支持)。
 *
 * @note    移植约束：
 *  1) send_char 必须“尽快返回”(例如写入发送 FIFO/DMA 队列)。它是在
 *     ymodem_receive_byte()/ymodem_tick() 的调用上下文中被同步调用的。
 *     若把 ymodem_receive_byte 挂在串口中断里，则所有回调
 *     (on_write/on_file_open/...) 都会在中断上下文执行 —— flash/文件系统
 *     等耗时或不可重入的操作应放到任务/主循环里，即：中断仅将收到的字节
 *     放入环形缓冲，再由循环取出并调用 ymodem_receive_byte()。
 *  2) 一个 STX 帧缓冲(1029B)内嵌于 ymodem_ctx_t。请勿把 ctx 定义为栈上
 *     局部变量，应使用 static/全局或堆分配。
 *  3) 回调必填项由 ymodem_init() 按角色校验：接收端必须提供
 *     on_file_open/on_write；发送端必须提供 on_tx_file_open/on_tx_read。
 *     on_file_close/on_transfer_end 可选。
 */

#ifndef YMODEM_H
#define YMODEM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Ymodem 协议控制字符
 * ================================================================ */
#define YMODEM_SOH      (0x01U) /* 128 字节数据帧起始 */
#define YMODEM_STX      (0x02U) /* 1024 字节数据帧起始 */
#define YMODEM_EOT      (0x04U) /* 传输结束 */
#define YMODEM_ACK      (0x06U) /* 确认 */
#define YMODEM_NAK      (0x15U) /* 否认/重传请求 */
#define YMODEM_CAN      (0x18U) /* 取消 */
#define YMODEM_C        (0x43U) /* 'C'：请求 CRC-16 模式 */
#define YMODEM_CTRLZ    (0x1AU) /* 文件末尾填充字符(Ctrl-Z) */

/* ================================================================
 * 帧尺寸定义
 * ================================================================ */
#define YMODEM_PACKET_SOH_SIZE (128U)                        /* SOH 载荷 */
#define YMODEM_PACKET_STX_SIZE (1024U)                       /* STX 载荷 */
#define YMODEM_SOH_FRAME_SIZE  (133U)  /* 1+1+1+128+2  */
#define YMODEM_STX_FRAME_SIZE  (1029U) /* 1+1+1+1024+2 */
#define YMODEM_MAX_FRAME_SIZE  YMODEM_STX_FRAME_SIZE

/* ================================================================
 * 可调超时/重试参数(毫秒)，可在编译期覆盖
 * ================================================================ */
#ifndef YMODEM_RX_START_TIMEOUT_MS      /* 接收端：等待对端首包/下一文件头 */
#define YMODEM_RX_START_TIMEOUT_MS (3000U)
#endif
#ifndef YMODEM_RX_DATA_TIMEOUT_MS       /* 接收端：数据帧之间的最大静默 */
#define YMODEM_RX_DATA_TIMEOUT_MS  (10000U)
#endif
#ifndef YMODEM_RX_FRAME_GAP_MS          /* 接收端：帧内两字节间静默超过此值即丢弃半帧重新同步 */
#define YMODEM_RX_FRAME_GAP_MS    (1000U)
#endif
#ifndef YMODEM_RX_MAX_INIT_RETRY        /* 接收端：握手阶段重发 'C' 的最大次数 */
#define YMODEM_RX_MAX_INIT_RETRY   (20U)
#endif
#ifndef YMODEM_RX_MAX_NAK_RETRY         /* 接收端：连续坏帧(反码/CRC 错)NAK 上限，超过即中止 */
#define YMODEM_RX_MAX_NAK_RETRY    (10U)
#endif
#ifndef YMODEM_TX_WAIT_C_TIMEOUT_MS     /* 发送端：等待对端首个 'C' */
#define YMODEM_TX_WAIT_C_TIMEOUT_MS (60000U)
#endif
#ifndef YMODEM_TX_RESP_TIMEOUT_MS       /* 发送端：等待 ACK/NAK/C 的响应超时 */
#define YMODEM_TX_RESP_TIMEOUT_MS   (5000U)
#endif
#ifndef YMODEM_TX_MAX_RETRY             /* 发送端：单包最大重传次数 */
#define YMODEM_TX_MAX_RETRY         (10U)
#endif
#ifndef YMODEM_CAN_ABORT_COUNT          /* 连续收到几个 CAN 视为对端中止 */
#define YMODEM_CAN_ABORT_COUNT      (2U)
#endif

/* ================================================================
 * 角色 / 结果 / 状态
 * ================================================================ */
typedef enum {
    YMODEM_MODE_INACTIVE = 0,
    YMODEM_MODE_RECEIVER,   /* 本端为接收方 */
    YMODEM_MODE_SENDER,     /* 本端为发送方 */
} ymodem_mode_t;

typedef enum {
    YMODEM_OK = 0,
    YMODEM_ERR_TIMEOUT,     /* 超时 */
    YMODEM_ERR_CANCELLED,   /* 对端 CAN 中止 */
    YMODEM_ERR_CRC,         /* 校验错误(不可恢复时上报) */
    YMODEM_ERR_SEQ,         /* 序号错误(不可恢复) */
    YMODEM_ERR_FILE_OPEN,   /* 文件打开/创建失败 */
    YMODEM_ERR_FILE_WRITE,  /* 文件读写失败 */
    YMODEM_ERR_ABORT,       /* 本地主动中止 */
    YMODEM_ERR_PARAM,       /* 参数错误 */
} ymodem_result_t;

typedef enum {
    YMODEM_STATE_IDLE = 0,      /* 未启动 */

    /* ---- 接收端 ---- */
    YMODEM_RX_WAIT_HEADER,      /* 已发 'C'，等待文件头包(seq 0) */
    YMODEM_RX_DATA,             /* 正在接收数据包(seq>=1，按模 256 回绕) */
    YMODEM_RX_WAIT_EOT2,        /* 收到第 1 个 EOT 并回 NAK，等待第 2 个 EOT */

    /* ---- 发送端 ---- */
    YMODEM_TX_WAIT_C,           /* 等待对端 'C' 以开始 */
    YMODEM_TX_WAIT_HDR_ACK,     /* 已发文件头包(0)，等待 ACK */
    YMODEM_TX_WAIT_HDR_C,       /* 已收 ACK，等待 'C' */
    YMODEM_TX_DATA,             /* 发送数据帧中，逐包等待 ACK */
    YMODEM_TX_WAIT_EOT_NAK,     /* 已发第 1 个 EOT，等待 NAK */
    YMODEM_TX_WAIT_EOT_ACK,     /* 已发第 2 个 EOT，等待 ACK */
    YMODEM_TX_WAIT_FIN_C,       /* 等待 'C'：决定下一文件 or 结束会话 */
    YMODEM_TX_WAIT_FIN_ACK,     /* 已发结束空包头，等待 ACK */

    YMODEM_STATE_DONE,          /* 会话成功结束 */
    YMODEM_STATE_ERROR,         /* 会话异常结束 */
} ymodem_state_t;

/* ================================================================
 * 回调接口
 * ================================================================ */
typedef struct ymodem_ctx ymodem_ctx_t;

typedef struct {
    /* ---------- 底层收发 ---------- */
    /* 将单字节写入发送通道，须尽快返回(见文件头说明) */
    void (*send_char)(ymodem_ctx_t *ctx, uint8_t ch);

    /* ---------- 接收端回调(必填) ---------- */
    /* 收到文件头时回调，创建/打开目标文件。返回 0 继续，非 0 拒绝并中止。*/
    int  (*on_file_open)(ymodem_ctx_t *ctx, const char *filename, uint32_t filesize);
    /* 收到数据块时回调。offset 为文件内偏移，len 为本次有效字节数(已按文件
       大小截断)。返回 0 成功，非 0 中止。*/
    int  (*on_write)(ymodem_ctx_t *ctx, uint32_t offset, const uint8_t *data, uint32_t len);
    /* 单个文件结束(正常或错误)时回调，用于 flush/close。result 为该文件结果。*/
    void (*on_file_close)(ymodem_ctx_t *ctx, ymodem_result_t result);

    /* ---------- 发送端回调(必填) ---------- */
    /* 准备下一个待发送文件。
       - 成功：把文件名写入 filename_out(至多 filename_max-1 字节 + '\0')，
         写入 *filesize_out，返回 0(表示“有文件要发”)。
       - 无更多文件：返回 1(发送端将发空包头结束整个会话)。
       - 出错/取消：返回负值(会话以错误终止)。 */
    int  (*on_tx_file_open)(ymodem_ctx_t *ctx, char *filename_out,
                            uint32_t filename_max, uint32_t *filesize_out);
    /* 读取当前文件数据到 buf，最多 want 字节；返回实际读取字节数，0 表示
       文件结尾(EOF)。允许短读(如按页读 flash)：中间件会在单帧内循环
       调用直至读满或 EOF，再以 0x1A 填充帧尾，因此短读不会污染帧数据。*/
    uint32_t (*on_tx_read)(ymodem_ctx_t *ctx, uint8_t *buf, uint32_t want);

    /* ---------- 会话级回调(可选) ---------- */
    /* 整个 Ymodem 会话结束(成功或失败)时回调一次。*/
    void (*on_transfer_end)(ymodem_ctx_t *ctx, ymodem_result_t result);

} ymodem_ops_t;

/* ================================================================
 * 状态机上下文
 * ================================================================ */
struct ymodem_ctx {
    ymodem_mode_t   mode;
    ymodem_state_t  state;
    const ymodem_ops_t *ops;
    void           *user_data;

    /* 帧重组 */
    uint8_t  frame_buf[YMODEM_MAX_FRAME_SIZE];
    uint16_t frame_index;   /* 已累计字节数(含起始字符) */
    uint16_t frame_target;  /* 目标帧长(SOH=133 / STX=1029) */

    /* 传输簿记 */
    uint8_t  expected_seq;  /* 接收端：期望的下一帧序号(按模 256 回绕) */
    uint32_t file_size;     /* 当前文件总大小 */
    uint32_t file_written;  /* 接收端：已写入字节数 */
    bool     file_size_valid; /* 文件大小字段是否有效(0 表示未知，不做截断) */
    bool     file_opened;   /* 接收端：当前是否有已打开文件(决定 close 调用) */

    /* 发送端 */
    uint8_t  tx_seq;        /* 发送端：上一帧序号(按模 256 回绕) */
    uint8_t  tx_code;       /* 发送端：上一帧类型(SOH/STX)，用于重发 */
    uint32_t tx_written;    /* 发送端：已送入帧的字节数(仅记录用) */

    /* 定时 / 重试 */
    uint32_t timer_ms;      /* 静默计时：仅在有效协议事件时清零 */
    uint16_t retry_count;   /* 握手/重传计数 */
    uint16_t nak_count;     /* 接收端：连续坏帧 NAK 计数 */
    uint8_t  cancel_count;  /* 连续 CAN 计数(收发两端统一要求连续 N 个) */
};

/* ================================================================
 * 对外 API
 * ================================================================ */

/**
 * @brief 初始化上下文(不启动传输)
 * @return 0 成功；-1 参数错误(含角色必填回调缺失)
 */
int ymodem_init(ymodem_ctx_t *ctx, ymodem_mode_t mode, const ymodem_ops_t *ops);

/**
 * @brief 启动会话。接收端会立即发出第一个 'C' 进入握手；发送端进入等待 'C'。
 */
void ymodem_start(ymodem_ctx_t *ctx);

/**
 * @brief 喂入一个从对端收到的字节，驱动状态机。
 */
void ymodem_receive_byte(ymodem_ctx_t *ctx, uint8_t ch);

/**
 * @brief 周期时基，驱动超时与重传。可在 SysTick/定时器/任务循环中调用。
 * @param elapsed_ms 距上次调用经过的毫秒数
 */
void ymodem_tick(ymodem_ctx_t *ctx, uint32_t elapsed_ms);

/**
 * @brief 本地主动中止会话(发送 CAN 并上报 YMODEM_ERR_ABORT)。
 */
void ymodem_abort(ymodem_ctx_t *ctx);

/**
 * @brief 便捷取数：当前状态(便于上层判断会话是否结束)
 */
static inline ymodem_state_t ymodem_get_state(ymodem_ctx_t *ctx)
{
    return ctx ? ctx->state : YMODEM_STATE_IDLE;
}

#ifdef __cplusplus
}
#endif

#endif /* YMODEM_H */
