/**
 * @file ymodem.h
 * @brief Ymodem 协议解析核心头文件
 * @note 纯 C 语言实现，完全不依赖特定硬件与操作系统，使用回调机制实现平台隔离与解耦。
 */

#ifndef YMODEM_H
#define YMODEM_H

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 * Ymodem 协议控制字符定义
 * ================================================================ */
#define YMODEM_SOH (0x01U)  /* 128 字节数据包起始字符 */
#define YMODEM_STX (0x02U)  /* 1024 字节数据包起始字符 */
#define YMODEM_EOT (0x04U)  /* 传输结束标记 */
#define YMODEM_ACK (0x06U)  /* 确认应答 */
#define YMODEM_NAK (0x15U)  /* 否认/重传请求 */
#define YMODEM_CAN (0x18U)  /* 取消传输 */
#define YMODEM_C   (0x43U)  /* ASCII 'C'，请求使用 CRC-16 校验模式 */

/* ================================================================
 * Ymodem 数据包尺寸定义
 * ================================================================ */
#define YMODEM_PACKET_SOH_SIZE (128U)
#define YMODEM_PACKET_STX_SIZE (1024U)

/* SOH 帧总长度 = 1(SOH) + 1(Seq) + 1(~Seq) + 128(Payload) + 2(CRC16) = 133 */
#define YMODEM_SOH_FRAME_SIZE  (133U)
/* STX 帧总长度 = 1(STX) + 1(Seq) + 1(~Seq) + 1024(Payload) + 2(CRC16) = 1029 */
#define YMODEM_STX_FRAME_SIZE  (1029U)

/* 最大单包缓存大小 */
#define YMODEM_MAX_FRAME_SIZE  (YMODEM_STX_FRAME_SIZE)

/* ================================================================
 * Ymodem 状态及结果定义
 * ================================================================ */
typedef enum
{
    YMODEM_STATE_OFFLINE = 0,   /* 未启动状态 */
    YMODEM_STATE_INIT,          /* 握手状态：循环发送 'C'，等待第一个包 */
    YMODEM_STATE_RECEIVING,     /* 数据接收状态：处理 SOH / STX 报文 */
    YMODEM_STATE_WAIT_EOT2,     /* 收到第一个 EOT，应答 NAK，等待第二个 EOT */
    YMODEM_STATE_CLOSED         /* 传输结束 */
} ymodem_state_t;

typedef enum
{
    YMODEM_OK = 0,
    YMODEM_ERR_TIMEOUT,         /* 超时退出 */
    YMODEM_ERR_CANCELLED,       /* 被发送方或接收方中止 */
    YMODEM_ERR_CRC,             /* 校验错误 */
    YMODEM_ERR_SEQ,             /* 序号错误 */
    YMODEM_ERR_FILE_OPEN,       /* 文件打开失败 */
    YMODEM_ERR_FILE_WRITE,      /* 文件写入错误 */
    YMODEM_ERR_ABORT            /* 外部原因退出 */
} ymodem_result_t;

/* ================================================================
 * 接口回调定义
 * ================================================================ */
typedef struct ymodem_ctx ymodem_ctx_t;

typedef struct
{
    /**
     * @brief 发送单字节数据接口
     */
    void (*send_char)(ymodem_ctx_t *ctx, uint8_t ch);

    /**
     * @brief 接收到文件头信息的回调
     * @param filename 文件名字符串
     * @param filesize 文件大小（字节数，若无法获取则为 0）
     * @return int 成功返回 0，返回非 0 值表示拒绝并中止接收
     */
    int (*on_file_header)(ymodem_ctx_t *ctx, const char *filename, uint32_t filesize);

    /**
     * @brief 接收到有效数据块的回调
     * @param data 数据缓冲区
     * @param offset 当前数据块在文件中的偏移量
     * @param size 本次数据块的实际字节数（已自动根据文件总大小截断填充的无用字符）
     * @return int 成功返回 0，返回非 0 值表示写入失败并中止接收
     */
    int (*on_data_block)(ymodem_ctx_t *ctx, const uint8_t *data, uint32_t offset, uint32_t size);

    /**
     * @brief 传输结束或异常中止回调
     * @param result 传输结果状态码
     */
    void (*on_transfer_end)(ymodem_ctx_t *ctx, ymodem_result_t result);
} ymodem_ops_t;

/* ================================================================
 * Ymodem 状态机上下文
 * ================================================================ */
struct ymodem_ctx
{
    ymodem_state_t state;
    const ymodem_ops_t *ops;
    void *user_data;                /* 用户上下文指针 */

    uint32_t file_size;             /* 文件总大小 */
    uint32_t write_offset;          /* 当前文件写偏移量 */
    uint8_t expected_seq;           /* 期望的数据包序列号 */
    
    /* 接收缓存及状态 */
    uint8_t frame_buf[YMODEM_MAX_FRAME_SIZE];
    uint16_t frame_index;
    uint16_t frame_target_size;

    /* 定时控制 */
    uint32_t timer_ms;              /* 定时计数器（ms） */
    uint8_t init_retry_count;       /* 发送 'C' 的重试次数 */
    uint8_t cancel_count;           /* 接收到的 CAN 字符计数 */
};

/* ================================================================
 * 协议栈对外 API
 * ================================================================ */

/**
 * @brief 初始化 Ymodem 状态机
 * @param ctx 状态机上下文句柄
 * @param ops 用户实现的回调接口体
 * @return int 成功返回 0，失败返回非 0
 */
int ymodem_init(ymodem_ctx_t *ctx, const ymodem_ops_t *ops);

/**
 * @brief 推入一个字节给 Ymodem 解析器
 * @param ctx 状态机上下文句柄
 * @param ch 接收到的字节
 */
void ymodem_receive_byte(ymodem_ctx_t *ctx, uint8_t ch);

/**
 * @brief 周期性调用 Tick 驱动超时及重试逻辑
 * @param ctx 状态机上下文句柄
 * @param ms_elapsed 距离上次调用过去的时间（毫秒）
 */
void ymodem_tick(ymodem_ctx_t *ctx, uint32_t ms_elapsed);

/**
 * @brief 主动中止当前传输
 * @param ctx 状态机上下文句柄
 */
void ymodem_abort(ymodem_ctx_t *ctx);

#endif /* YMODEM_H */
