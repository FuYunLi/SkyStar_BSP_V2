/**
 * @file ymodem.c
 * @brief Ymodem 协议解析核心源文件
 * @note 纯 C 语言实现，完全不依赖特定硬件与操作系统，使用回调机制实现底层隔离与解耦。
 */

#include "ymodem.h"
#include <string.h>
#include <stdlib.h>

/* ================================================================
 * 私有宏定义与常量
 * ================================================================ */
#define YMODEM_INIT_TIMEOUT_MS     (3000U)  /* 发送 'C' 的时间间隔 (3s) */
#define YMODEM_DATA_TIMEOUT_MS     (10000U) /* 数据传输过程中的超时时间 (10s) */
#define YMODEM_MAX_INIT_RETRIES    (20U)    /* 最大初始握手重试次数 */
#define YMODEM_CAN_ABORT_COUNT     (2U)     /* 连续收到几个 CAN 中止传输 */

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

/**
 * @brief 计算 CRC-16 CCITT 校验值
 * @note 多项式为 0x1021，初始值为 0x0000
 */
static uint16_t crc16_ccitt(const uint8_t *data, uint32_t size)
{
    uint16_t crc = 0U;
    
    for (uint32_t idx = 0U; idx < size; ++idx)
    {
        crc ^= (uint16_t)data[idx] << 8;
        for (int i = 0; i < 8; ++i)
        {
            if (crc & 0x8000U)
            {
                crc = (crc << 1) ^ 0x1021U;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

/**
 * @brief 向发送方发送连续的 CAN 字符以中止传输
 */
static void send_can_abort(ymodem_ctx_t *ctx)
{
    if (ctx != NULL && ctx->ops != NULL && ctx->ops->send_char != NULL)
    {
        for (int i = 0; i < 5; ++i)
        {
            ctx->ops->send_char(ctx, YMODEM_CAN);
        }
    }
}

/* ================================================================
 * 协议栈 API 实现
 * ================================================================ */

/**
 * @brief 初始化 Ymodem 状态机
 */
int ymodem_init(ymodem_ctx_t *ctx, const ymodem_ops_t *ops)
{
    if (ctx == NULL || ops == NULL || ops->send_char == NULL)
    {
        return -1;
    }

    memset(ctx, 0, sizeof(ymodem_ctx_t));
    ctx->ops = ops;
    ctx->state = YMODEM_STATE_OFFLINE;
    
    return 0;
}

/**
 * @brief 主动中止当前传输
 */
void ymodem_abort(ymodem_ctx_t *ctx)
{
    if (ctx == NULL || ctx->state == YMODEM_STATE_OFFLINE)
    {
        return;
    }

    send_can_abort(ctx);
    ctx->state = YMODEM_STATE_OFFLINE;
    
    if (ctx->ops->on_transfer_end != NULL)
    {
        ctx->ops->on_transfer_end(ctx, YMODEM_ERR_ABORT);
    }
}

/**
 * @brief 接收单字节并驱动状态机
 */
void ymodem_receive_byte(ymodem_ctx_t *ctx, uint8_t ch)
{
    if (ctx == NULL || ctx->ops == NULL)
    {
        return;
    }

    /* 处于离线状态时忽略所有字符，除非是启动初始化 */
    if (ctx->state == YMODEM_STATE_OFFLINE)
    {
        return;
    }

    /* 重置数据接收超时计时器 */
    ctx->timer_ms = 0U;

    /* 检测到 CAN 中断符 (仅在等待帧起始字符时有效，防止数据载荷中的 0x18 被误判为 CAN 信号) */
    if (ctx->frame_index == 0U && ch == YMODEM_CAN)
    {
        ctx->cancel_count++;
        if (ctx->cancel_count >= YMODEM_CAN_ABORT_COUNT)
        {
            ctx->state = YMODEM_STATE_OFFLINE;
            if (ctx->ops->on_transfer_end != NULL)
            {
                ctx->ops->on_transfer_end(ctx, YMODEM_ERR_CANCELLED);
            }
            return;
        }
    }
    else if (ctx->frame_index == 0U)
    {
        ctx->cancel_count = 0U;
    }

    /* 帧解析逻辑 */
    if (ctx->frame_index == 0U)
    {
        /* 接收帧起始字符 */
        if (ch == YMODEM_SOH)
        {
            ctx->frame_buf[0] = ch;
            ctx->frame_index = 1U;
            ctx->frame_target_size = YMODEM_SOH_FRAME_SIZE;
        }
        else if (ch == YMODEM_STX)
        {
            ctx->frame_buf[0] = ch;
            ctx->frame_index = 1U;
            ctx->frame_target_size = YMODEM_STX_FRAME_SIZE;
        }
        else if (ch == YMODEM_EOT)
        {
            /* 处理传输结束标志 */
            if (ctx->state == YMODEM_STATE_RECEIVING)
            {
                ctx->state = YMODEM_STATE_WAIT_EOT2;
                ctx->ops->send_char(ctx, YMODEM_NAK);
            }
            else if (ctx->state == YMODEM_STATE_WAIT_EOT2)
            {
                ctx->ops->send_char(ctx, YMODEM_ACK);
                /* 准备接收下一个文件（如果有的话），发送 'C' 启动 */
                ctx->expected_seq = 0U;
                ctx->state = YMODEM_STATE_INIT;
                ctx->ops->send_char(ctx, YMODEM_C);
            }
        }
    }
    else
    {
        /* 缓存帧内数据 */
        ctx->frame_buf[ctx->frame_index] = ch;
        ctx->frame_index++;

        /* 帧接收完整，开始校验解析 */
        if (ctx->frame_index >= ctx->frame_target_size)
        {
            uint8_t seq = ctx->frame_buf[1];
            uint8_t seq_inv = ctx->frame_buf[2];
            uint16_t crc_received;
            uint16_t crc_computed;
            uint16_t payload_size = (ctx->frame_buf[0] == YMODEM_SOH) ? YMODEM_PACKET_SOH_SIZE : YMODEM_PACKET_STX_SIZE;

            /* 1. 验证序列号 */
            if ((uint8_t)(seq + seq_inv) != 0xFFU)
            {
                ctx->ops->send_char(ctx, YMODEM_NAK);
                ctx->frame_index = 0U;
                return;
            }

            /* 2. 校验 CRC */
            crc_received = ((uint16_t)ctx->frame_buf[ctx->frame_target_size - 2] << 8) | ctx->frame_buf[ctx->frame_target_size - 1];
            crc_computed = crc16_ccitt(&ctx->frame_buf[3], payload_size);
            if (crc_computed != crc_received)
            {
                ctx->ops->send_char(ctx, YMODEM_NAK);
                ctx->frame_index = 0U;
                return;
            }

            /* 3. 处理序列号状态流转 */
            if (seq == ctx->expected_seq)
            {
                if (seq == 0U)
                {
                    /* Packet 0：元数据包（文件名与大小） */
                    if (ctx->frame_buf[3] == 0U)
                    {
                        /* 全 0 包表示整个 Ymodem 会话结束 */
                        ctx->ops->send_char(ctx, YMODEM_ACK);
                        ctx->state = YMODEM_STATE_CLOSED;
                        if (ctx->ops->on_transfer_end != NULL)
                        {
                            ctx->ops->on_transfer_end(ctx, YMODEM_OK);
                        }
                    }
                    else
                    {
                        /* 解析文件名和大小 */
                        const char *filename = (const char *)&ctx->frame_buf[3];
                        uint32_t file_len = 0U;
                        
                        /* 文件名后以 '\0' 隔开紧接文件大小字符串 */
                        uint32_t name_len = strlen(filename);
                        if (name_len < payload_size - 1U)
                        {
                            const char *size_str = &filename[name_len + 1];
                            file_len = (uint32_t)strtoul(size_str, NULL, 10);
                        }

                        ctx->file_size = file_len;
                        ctx->write_offset = 0U;

                        /* 触发应用层建文件回调 */
                        if (ctx->ops->on_file_header != NULL && 
                            ctx->ops->on_file_header(ctx, filename, file_len) == 0)
                        {
                            ctx->ops->send_char(ctx, YMODEM_ACK);
                            /* 按 Ymodem 规范，ACK 之后发送 'C' 启动正式数据包 1 的接收 */
                            ctx->ops->send_char(ctx, YMODEM_C);
                            ctx->expected_seq = 1U;
                            ctx->state = YMODEM_STATE_RECEIVING;
                        }
                        else
                        {
                            /* 用户拒绝接收（如存储空间不足或创建文件失败） */
                            send_can_abort(ctx);
                            ctx->state = YMODEM_STATE_OFFLINE;
                            if (ctx->ops->on_transfer_end != NULL)
                            {
                                ctx->ops->on_transfer_end(ctx, YMODEM_ERR_FILE_OPEN);
                            }
                        }
                    }
                }
                else
                {
                    /* 正常数据包 */
                    uint32_t write_size = payload_size;

                    /* 自动根据剩余文件大小进行截断（剥离尾部填充的无用字符） */
                    if (ctx->file_size > 0U)
                    {
                        uint32_t remaining = ctx->file_size - ctx->write_offset;
                        if (write_size > remaining)
                        {
                            write_size = remaining;
                        }
                    }

                    if (write_size > 0U)
                    {
                        if (ctx->ops->on_data_block != NULL && 
                            ctx->ops->on_data_block(ctx, &ctx->frame_buf[3], ctx->write_offset, write_size) == 0)
                        {
                            ctx->write_offset += write_size;
                            ctx->ops->send_char(ctx, YMODEM_ACK);
                            ctx->expected_seq++;
                        }
                        else
                        {
                            /* 写入存储器出错 */
                            send_can_abort(ctx);
                            ctx->state = YMODEM_STATE_OFFLINE;
                            if (ctx->ops->on_transfer_end != NULL)
                            {
                                ctx->ops->on_transfer_end(ctx, YMODEM_ERR_FILE_WRITE);
                            }
                        }
                    }
                    else
                    {
                        /* 已写完，仅做 ACK 应答 */
                        ctx->ops->send_char(ctx, YMODEM_ACK);
                        ctx->expected_seq++;
                    }
                }
            }
            else if (seq == (uint8_t)(ctx->expected_seq - 1U))
            {
                /* 收到发送方因没拿到 ACK 而重发的上一个重复数据包，直接重新回复 ACK 即可 */
                ctx->ops->send_char(ctx, YMODEM_ACK);
            }
            else
            {
                /* 严重的序号错误，中止传输 */
                send_can_abort(ctx);
                ctx->state = YMODEM_STATE_OFFLINE;
                if (ctx->ops->on_transfer_end != NULL)
                {
                    ctx->ops->on_transfer_end(ctx, YMODEM_ERR_SEQ);
                }
            }

            ctx->frame_index = 0U; /* 重置以接收下一帧 */
        }
    }
}

/**
 * @brief 周期性调用 Tick 驱动超时及重试逻辑
 */
void ymodem_tick(ymodem_ctx_t *ctx, uint32_t ms_elapsed)
{
    if (ctx == NULL || ctx->state == YMODEM_STATE_OFFLINE)
    {
        return;
    }

    ctx->timer_ms += ms_elapsed;

    if (ctx->state == YMODEM_STATE_INIT)
    {
        /* 握手阶段：3秒没有收到响应则重发 'C' */
        if (ctx->timer_ms >= YMODEM_INIT_TIMEOUT_MS)
        {
            ctx->timer_ms = 0U;
            ctx->init_retry_count++;
            
            if (ctx->init_retry_count >= YMODEM_MAX_INIT_RETRIES)
            {
                ctx->state = YMODEM_STATE_OFFLINE;
                if (ctx->ops->on_transfer_end != NULL)
                {
                    ctx->ops->on_transfer_end(ctx, YMODEM_ERR_TIMEOUT);
                }
            }
            else
            {
                ctx->ops->send_char(ctx, YMODEM_C);
            }
        }
    }
    else
    {
        /* 传输阶段：10秒未收到任何数据，判定为超时中止 */
        if (ctx->timer_ms >= YMODEM_DATA_TIMEOUT_MS)
        {
            send_can_abort(ctx);
            ctx->state = YMODEM_STATE_OFFLINE;
            if (ctx->ops->on_transfer_end != NULL)
            {
                ctx->ops->on_transfer_end(ctx, YMODEM_ERR_TIMEOUT);
            }
        }
    }
}
