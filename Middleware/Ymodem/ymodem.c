/**
 * @file    ymodem.c
 * @brief   纯 C、平台无关、非阻塞的 Ymodem(YMODEM-CRC)协议中间件实现 (V3)
 *
 * V2 相对 V1 的修复(对应 V1 审查清单)：
 *  - [P0] 帧序号按模 256 回绕：文件头仅在 RX_WAIT_HEADER 状态下由 seq==0
 *    判定；RX_DATA 中回绕到 0 的 seq 是普通数据帧。V1 中 >255 帧的文件
 *    (STX 下 >261,120 字节)第 256 帧会被误判为文件头，载荷首字节为
 *    0x00 时甚至静默截断并上报成功。
 *  - [P1] 接收端坏帧(反码/CRC 错)NAK 设连续上限，超限发 CAN 中止，
 *    消除“链路持续坏数据时无限 NAK”的活锁。
 *  - [P1] 静默计时只在有效协议事件(通过校验的完整帧/EOT)时清零，
 *    噪声字节不再刷新超时；发送端同理(仅 ACK/NAK/'C' 等推进事件清零)。
 *  - [P1] 接收端增加帧内断流重同步：半帧静默超过 YMODEM_RX_FRAME_GAP_MS
 *    即丢弃，等对端重发恢复，不再依赖“凑满后 CRC 错”的慢速恢复。
 *  - [P2] 发送端 TX_WAIT_FIN_C 超时视为会话正常结束(所有文件已传完，
 *    部分接收端收完即静默退出，不再误报 YMODEM_ERR_TIMEOUT)。
 *  - [P2] on_tx_read 允许短读：单帧内循环读取直至读满或 EOF，短读
 *    (如 flash 按页读)不再污染帧尾导致文件内容错位。
 *  - [P3] 发送端 CAN 判定与接收端对称：须连续 YMODEM_CAN_ABORT_COUNT 个。
 *  - [P3] CRC 注释更正为 CRC-16/XMODEM(init 0x0000，非 CCITT-FALSE)。
 *  - [P3] ymodem_init 按角色校验必填回调；header 尺寸解析增加溢出防护。
 *
 * V3 相对 V2 的修复：
 *  - [P1] 帧内断流重同步计时修正：接收端在帧内每收到一个字节即刷新
 *    静默计时，使 YMODEM_RX_FRAME_GAP_MS 度量的是"帧内两字节间静默"而非
 *    "距上一帧完成时间"。修复 V2 在低波特率(如 9600bps)下整帧接收耗时
 *    (>1s)会被误判为半帧断流而丢弃正常长帧的问题。
 */

#include "ymodem.h"
#include <string.h>

/* ================================================================
 * 私有：CRC-16/XMODEM (Ymodem 规范：poly 0x1021, init 0x0000, MSB-first)
 * ================================================================ */
static uint16_t crc16_xmodem(const uint8_t *data, uint32_t size)
{
    uint16_t crc = 0x0000U;

    for (uint32_t idx = 0U; idx < size; ++idx)
    {
        crc ^= (uint16_t)((uint16_t)data[idx] << 8);
        for (int i = 0; i < 8; ++i)
        {
            if (crc & 0x8000U)
            {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* ================================================================
 * 私有：通用小工具
 * ================================================================ */
static inline void put_char(ymodem_ctx_t *ctx, uint8_t ch)
{
    if (ctx->ops->send_char != NULL)
    {
        ctx->ops->send_char(ctx, ch);
    }
}

static void send_can_abort(ymodem_ctx_t *ctx)
{
    for (int i = 0; i < 5; ++i)
    {
        put_char(ctx, YMODEM_CAN);
    }
}

static void frame_reset(ymodem_ctx_t *ctx)
{
    ctx->frame_index  = 0U;
    ctx->frame_target = 0U;
}

/* 自带定长求长，避免依赖 POSIX 的 strnlen(部分 ARMCC/裸机库不提供) */
static uint32_t ym_strnlen(const char *s, uint32_t maxlen)
{
    uint32_t n = 0U;
    while (n < maxlen && s[n] != '\0')
    {
        ++n;
    }
    return n;
}

/* 从头包载荷("filename\0size\0...")解析文件大小；
   返回解析到的十进制值，并通过 out_valid 指示是否为有效非零大小。*/
static uint32_t parse_header_size(const uint8_t *payload, uint32_t payload_size,
                                  const char **out_name, bool *out_valid)
{
    const char *name = (const char *)payload;
    uint32_t    sz   = 0U;
    bool        valid = false;

    /* 载荷未必以 '\0' 结尾，这里靠 YMODEM_MAX_FRAME_SIZE 边界内的内容，
       payload[0..] 已由发送方保证以 '\0' 分隔文件名与大小。*/
    *out_name = name;
    uint32_t name_len = ym_strnlen(name, payload_size);
    if (name_len < payload_size)
    {
        const char *p = (const char *)payload + name_len + 1U;
        /* 十进制解析，遇非数字或溢出停止 */
        while ((uint32_t)(p - (const char *)payload) < payload_size)
        {
            if (*p >= '0' && *p <= '9')
            {
                uint32_t digit = (uint32_t)(*p - '0');
                if (sz > (UINT32_MAX - digit) / 10U)
                {
                    /* 溢出：视为大小未知(不截断)，由上层对写入量把关 */
                    sz = 0U;
                    break;
                }
                sz = sz * 10U + digit;
                ++p;
            }
            else
            {
                break;
            }
        }
        valid = (sz > 0U);
    }
    *out_valid = valid; /* V1 漏写此句：file_size_valid 曾是未初始化的栈值 */
    return sz;
}

/* ================================================================
 * 私有：结束处理
 * ================================================================ */
static void finish(ymodem_ctx_t *ctx, ymodem_result_t result)
{
    bool was_receiver = (ctx->mode == YMODEM_MODE_RECEIVER);

    /* 接收端：若仍有未关闭的文件，先关闭它 */
    if (was_receiver && ctx->file_opened)
    {
        if (ctx->ops->on_file_close != NULL)
        {
            ctx->ops->on_file_close(ctx, result);
        }
        ctx->file_opened = false;
    }

    ctx->state = (result == YMODEM_OK) ? YMODEM_STATE_DONE : YMODEM_STATE_ERROR;

    if (ctx->ops->on_transfer_end != NULL)
    {
        ctx->ops->on_transfer_end(ctx, result);
    }
}

static void fail(ymodem_ctx_t *ctx, ymodem_result_t result)
{
    send_can_abort(ctx);
    finish(ctx, result);
}

/* ================================================================
 * 私有：接收端帧处理
 * ================================================================ */
static void rx_handle_header_packet(ymodem_ctx_t *ctx, uint32_t payload_size)
{
    const char *name;
    bool        valid;
    uint32_t    size = parse_header_size(&ctx->frame_buf[3], payload_size, &name, &valid);

    if (ctx->frame_buf[3] == 0U)
    {
        /* 空包头：整个会话结束 */
        put_char(ctx, YMODEM_ACK);
        finish(ctx, YMODEM_OK);
        return;
    }

    /* 开始一个新文件(on_file_open 必填，init 已校验) */
    if (ctx->ops->on_file_open(ctx, name, size) == 0)
    {
        ctx->file_size       = size;
        ctx->file_size_valid = valid;
        ctx->file_written    = 0U;
        ctx->file_opened     = true;

        put_char(ctx, YMODEM_ACK);
        put_char(ctx, YMODEM_C);      /* 启动数据包(seq 1)传输 */
        ctx->expected_seq = 1U;
        ctx->state        = YMODEM_RX_DATA;
        ctx->retry_count  = 0U;
        ctx->nak_count    = 0U;
    }
    else
    {
        /* 应用拒绝(空间不足/创建失败) */
        fail(ctx, YMODEM_ERR_FILE_OPEN);
    }
}

static void rx_handle_data_packet(ymodem_ctx_t *ctx, uint16_t payload_size)
{
    uint32_t write_size = payload_size;

    /* 仅在文件大小已知时截断尾部填充；大小未知则原样写入 */
    if (ctx->file_size_valid)
    {
        uint32_t remaining = (ctx->file_written < ctx->file_size)
                                 ? (ctx->file_size - ctx->file_written) : 0U;
        if (write_size > remaining)
        {
            write_size = remaining;
        }
    }

    if (write_size > 0U)
    {
        if (ctx->ops->on_write(ctx, ctx->file_written, &ctx->frame_buf[3], write_size) == 0)
        {
            ctx->file_written += write_size;
            put_char(ctx, YMODEM_ACK);
            ctx->expected_seq++;
            ctx->retry_count  = 0U;
            ctx->nak_count    = 0U;
        }
        else
        {
            fail(ctx, YMODEM_ERR_FILE_WRITE);
        }
    }
    else
    {
        /* 已写满，多余填充帧仅回应不写入 */
        put_char(ctx, YMODEM_ACK);
        ctx->expected_seq++;
        ctx->retry_count  = 0U;
        ctx->nak_count    = 0U;
    }
}

static void rx_process_frame(ymodem_ctx_t *ctx)
{
    uint8_t  seq     = ctx->frame_buf[1];
    uint8_t  seq_inv = ctx->frame_buf[2];
    uint16_t payload_size = (ctx->frame_buf[0] == YMODEM_SOH)
                                ? YMODEM_PACKET_SOH_SIZE : YMODEM_PACKET_STX_SIZE;

    /* 1) 序号反码校验：坏帧 NAK 计数，连续超限中止 */
    if ((uint8_t)(seq + seq_inv) != 0xFFU)
    {
        if (++ctx->nak_count > YMODEM_RX_MAX_NAK_RETRY)
        {
            fail(ctx, YMODEM_ERR_CRC);
        }
        else
        {
            put_char(ctx, YMODEM_NAK);
            frame_reset(ctx);
        }
        return;
    }

    /* 2) CRC 校验：同上 */
    uint16_t crc_recv = (uint16_t)(((uint16_t)ctx->frame_buf[ctx->frame_target - 2] << 8) |
                                    ctx->frame_buf[ctx->frame_target - 1]);
    uint16_t crc_calc = crc16_xmodem(&ctx->frame_buf[3], payload_size);
    if (crc_recv != crc_calc)
    {
        if (++ctx->nak_count > YMODEM_RX_MAX_NAK_RETRY)
        {
            fail(ctx, YMODEM_ERR_CRC);
        }
        else
        {
            put_char(ctx, YMODEM_NAK);
            frame_reset(ctx);
        }
        return;
    }

    /* 完整有效帧到达：刷新静默计时(噪声/坏帧字节不刷新) */
    ctx->timer_ms = 0U;

    /* 3) 按状态区分语义：文件头只在等待文件头阶段出现；数据帧序号按模
          256 回绕，回绕到 0 仍是数据帧，绝不能仅凭 seq==0 判为文件头 */
    if (ctx->state == YMODEM_RX_WAIT_HEADER)
    {
        if (seq == 0U)
        {
            rx_handle_header_packet(ctx, payload_size);
        }
        else
        {
            /* 等待文件头阶段不接受任何数据帧(消除 V1 中 expected==0 时
               把 seq==255 当作“上一帧重发”误 ACK 的瑕疵) */
            fail(ctx, YMODEM_ERR_SEQ);
        }
    }
    else /* RX_DATA / RX_WAIT_EOT2 */
    {
        if (seq == ctx->expected_seq)
        {
            rx_handle_data_packet(ctx, payload_size);
        }
        else if (seq == (uint8_t)(ctx->expected_seq - 1U))
        {
            /* 对端没收到上一帧的 ACK 而重发：重新确认(不重写)。
               若重发的是文件头(seq 0，仅出现在 expected==1 时，即对端
               把 header 的 ACK+'C' 组合丢了)，还需补发 'C' 重启数据流。*/
            put_char(ctx, YMODEM_ACK);
            if (seq == 0U)
            {
                put_char(ctx, YMODEM_C);
            }
            ctx->nak_count = 0U;
        }
        else
        {
            /* 严重序号错误 */
            fail(ctx, YMODEM_ERR_SEQ);
        }
    }

    if (ctx->state != YMODEM_STATE_DONE && ctx->state != YMODEM_STATE_ERROR)
    {
        frame_reset(ctx);
    }
}

static void rx_feed_byte(ymodem_ctx_t *ctx, uint8_t ch)
{
    /* 只在活动状态处理 */
    if (ctx->state != YMODEM_RX_WAIT_HEADER &&
        ctx->state != YMODEM_RX_DATA &&
        ctx->state != YMODEM_RX_WAIT_EOT2)
    {
        return;
    }

    /* 帧边界处的 CAN 计数(避免误判载荷中的 0x18) */
    if (ctx->frame_index == 0U)
    {
        if (ch == YMODEM_CAN)
        {
            ctx->cancel_count++;
            if (ctx->cancel_count >= YMODEM_CAN_ABORT_COUNT)
            {
                finish(ctx, YMODEM_ERR_CANCELLED);
            }
            return;
        }
        ctx->cancel_count = 0U;
    }

    if (ctx->frame_index == 0U)
    {
        /* 等待帧起始字符 */
        if (ch == YMODEM_SOH)
        {
            ctx->frame_buf[0]  = ch;
            ctx->frame_index   = 1U;
            ctx->frame_target  = YMODEM_SOH_FRAME_SIZE;
        }
        else if (ch == YMODEM_STX)
        {
            ctx->frame_buf[0]  = ch;
            ctx->frame_index   = 1U;
            ctx->frame_target  = YMODEM_STX_FRAME_SIZE;
        }
        else if (ch == YMODEM_EOT)
        {
            if (ctx->state == YMODEM_RX_DATA)
            {
                /* 第 1 个 EOT：回 NAK 逼对端再发一次 */
                put_char(ctx, YMODEM_NAK);
                ctx->state    = YMODEM_RX_WAIT_EOT2;
                ctx->timer_ms = 0U;
            }
            else if (ctx->state == YMODEM_RX_WAIT_EOT2)
            {
                /* 第 2 个 EOT：确认，关闭当前文件，发 'C' 等下一文件头或空包 */
                put_char(ctx, YMODEM_ACK);
                if (ctx->file_opened)
                {
                    if (ctx->ops->on_file_close != NULL)
                    {
                        ctx->ops->on_file_close(ctx, YMODEM_OK);
                    }
                    ctx->file_opened = false;
                }
                ctx->expected_seq = 0U;
                ctx->state        = YMODEM_RX_WAIT_HEADER;
                ctx->retry_count  = 0U;
                ctx->nak_count    = 0U;
                ctx->timer_ms     = 0U;
                put_char(ctx, YMODEM_C);
                frame_reset(ctx);
            }
            /* IDLE/WAIT_HEADER 下收到 EOT 视为噪声，忽略 */
        }
        /* 其余字符(CAN 之外)忽略 */
    }
    else
    {
        /* 帧内数据累积 */
        ctx->frame_buf[ctx->frame_index] = ch;
        ctx->frame_index++;

        /* [V3] 帧内每收到一个字节即刷新静默计时：让 FRAME_GAP 度量的是
           "帧内两字节之间的静默"，而非"距上一帧完成的时间"。否则低
           波特率下一整帧的接收耗时会累加(如 9600bps 收 1029 字节
           需 ~1.07s > 默认 1000ms)，正常长帧会被误当半帧丢弃。
           帧边界(frame_index==0)的噪声字节仍不刷新计时(P1-3 语义不变)。*/
        ctx->timer_ms = 0U;

        if (ctx->frame_index >= ctx->frame_target)
        {
            rx_process_frame(ctx);
        }
    }
}

/* ================================================================
 * 私有：发送端
 * ================================================================ */
static void tx_send_packet(ymodem_ctx_t *ctx, uint8_t code, uint8_t seq)
{
    uint16_t payload_size = (code == YMODEM_SOH) ? YMODEM_PACKET_SOH_SIZE
                                                  : YMODEM_PACKET_STX_SIZE;
    uint16_t total = (uint16_t)(payload_size + 5U);
    uint16_t crc   = crc16_xmodem(&ctx->frame_buf[3], payload_size);

    ctx->frame_buf[0] = code;
    ctx->frame_buf[1] = seq;
    ctx->frame_buf[2] = (uint8_t)(~seq);
    ctx->frame_buf[total - 2U] = (uint8_t)(crc >> 8);
    ctx->frame_buf[total - 1U] = (uint8_t)(crc & 0xFFU);

    ctx->tx_code = code;
    ctx->tx_seq  = seq;

    for (uint16_t i = 0U; i < total; ++i)
    {
        put_char(ctx, ctx->frame_buf[i]);
    }
}

/* 组文件头包(seq 0)：filename\0decimal_size\0 存于 frame_buf[3]，其余清零 */
static void tx_build_and_send_header(ymodem_ctx_t *ctx, const char *filename, uint32_t size)
{
    uint8_t  *p   = &ctx->frame_buf[3];
    uint32_t  cap = YMODEM_PACKET_SOH_SIZE;
    char      ds[12];
    uint32_t  d = 0U;
    uint32_t  n;
    uint32_t  nmax;

    memset(p, 0x00, cap);

    /* 先把大小转成十进制 ASCII 字符串 */
    if (size == 0U)
    {
        ds[d++] = '0';
    }
    else
    {
        char     tmp[12];
        uint32_t t = 0U;
        while (size > 0U)
        {
            tmp[t++] = (char)('0' + (size % 10U));
            size /= 10U;
        }
        while (t > 0U)
        {
            ds[d++] = tmp[--t];
        }
    }
    ds[d] = '\0';

    /* 限制文件名长度，确保 name\0size\0 完整落在载荷区内，绝不越界到 CRC 区 */
    n    = ym_strnlen(filename, cap);
    nmax = cap - d - 2U;            /* 需要 name( n ) + '\0' + size(d) + '\0' <= cap */
    if (n > nmax)
    {
        n = nmax;
    }
    memcpy(p, filename, n);
    p[n] = 0x00U;
    memcpy(&p[n + 1U], ds, d + 1U);

    tx_send_packet(ctx, YMODEM_SOH, 0U);
}

/* 读取并发送下一数据帧；返回 false 表示已 EOF(调用方进入 EOT 流程)。
   回调允许短读(如按页读 flash)：帧内循环读取直至读满或 EOF，短读
   不会把填充字符插到文件数据中间(V1 的隐患)。*/
static bool tx_send_next_data(ymodem_ctx_t *ctx)
{
    uint8_t  *p     = &ctx->frame_buf[3];
    uint32_t  total = 0U;
    uint32_t  got;

    memset(p, YMODEM_CTRLZ, YMODEM_PACKET_STX_SIZE); /* 未读满的尾部即为 0x1A 填充 */

    while (total < YMODEM_PACKET_STX_SIZE)
    {
        got = ctx->ops->on_tx_read(ctx, &p[total], YMODEM_PACKET_STX_SIZE - total);
        if (got == 0U)
        {
            break; /* EOF */
        }
        total += got;
    }

    if (total == 0U)
    {
        return false; /* EOF */
    }

    ctx->tx_written += total;
    ctx->tx_seq++;
    tx_send_packet(ctx, YMODEM_STX, ctx->tx_seq);
    return true;
}

/* 依据当前等待状态重发上一帧(用于 ACK 丢失/超时) */
static void tx_resend_current(ymodem_ctx_t *ctx)
{
    switch (ctx->state)
    {
    case YMODEM_TX_WAIT_HDR_ACK:
        tx_send_packet(ctx, YMODEM_SOH, 0U);   /* 载荷仍在 frame_buf */
        break;
    case YMODEM_TX_DATA:
        tx_send_packet(ctx, YMODEM_STX, ctx->tx_seq);
        break;
    case YMODEM_TX_WAIT_EOT_NAK:
    case YMODEM_TX_WAIT_EOT_ACK:
        put_char(ctx, YMODEM_EOT);
        break;
    case YMODEM_TX_WAIT_FIN_ACK:
        tx_send_packet(ctx, YMODEM_SOH, 0U);   /* 空包头 */
        break;
    default:
        break;
    }
}

static void tx_enter_eot(ymodem_ctx_t *ctx)
{
    put_char(ctx, YMODEM_EOT);
    ctx->state       = YMODEM_TX_WAIT_EOT_NAK;
    ctx->timer_ms    = 0U;
    ctx->retry_count = 0U;
}

/* 会话结束时决定：发下一文件头，还是发空包头结束 */
static void tx_begin_or_end(ymodem_ctx_t *ctx)
{
    char     name[128];
    uint32_t size = 0U;
    int      r    = 1;

    name[0] = '\0';
    r = ctx->ops->on_tx_file_open(ctx, name, (uint32_t)sizeof(name), &size);

    if (r == 0)
    {
        /* 有文件：组并发送文件头包(0) */
        ctx->file_size   = size;
        ctx->tx_written  = 0U;
        ctx->tx_seq      = 0U;
        tx_build_and_send_header(ctx, (name[0] != '\0') ? name : "noname", size);
        ctx->state       = YMODEM_TX_WAIT_HDR_ACK;
        ctx->timer_ms    = 0U;
        ctx->retry_count = 0U;
    }
    else if (r > 0)
    {
        /* 无更多文件：发空包头(全 0)结束会话 */
        memset(&ctx->frame_buf[3], 0x00, YMODEM_PACKET_SOH_SIZE);
        tx_send_packet(ctx, YMODEM_SOH, 0U);
        ctx->state       = YMODEM_TX_WAIT_FIN_ACK;
        ctx->timer_ms    = 0U;
        ctx->retry_count = 0U;
    }
    else
    {
        fail(ctx, YMODEM_ERR_ABORT);
    }
}

static void tx_feed_byte(ymodem_ctx_t *ctx, uint8_t ch)
{
    /* 统一 CAN 判定：与接收端对称，须连续 YMODEM_CAN_ABORT_COUNT 个才中止，
       避免单个 0x18 噪声字节误杀会话(V1 中发送端单字节 CAN 即中止)。*/
    if (ch == YMODEM_CAN)
    {
        ctx->cancel_count++;
        if (ctx->cancel_count >= YMODEM_CAN_ABORT_COUNT)
        {
            finish(ctx, YMODEM_ERR_CANCELLED);
        }
        return;
    }
    ctx->cancel_count = 0U;

    switch (ctx->state)
    {
    case YMODEM_TX_WAIT_C:
        if (ch == YMODEM_C)
        {
            tx_begin_or_end(ctx);
        }
        break;

    case YMODEM_TX_WAIT_HDR_ACK:
        if (ch == YMODEM_ACK)
        {
            ctx->state       = YMODEM_TX_WAIT_HDR_C;
            ctx->timer_ms    = 0U;
            ctx->retry_count = 0U;
        }
        else if (ch == YMODEM_NAK)
        {
            tx_resend_current(ctx);
            ctx->timer_ms = 0U;
        }
        break;

    case YMODEM_TX_WAIT_HDR_C:
        if (ch == YMODEM_C)
        {
            if (!tx_send_next_data(ctx))
            {
                tx_enter_eot(ctx); /* 空文件：无数据直接 EOT */
            }
            else
            {
                ctx->state       = YMODEM_TX_DATA;
                ctx->timer_ms    = 0U;
                ctx->retry_count = 0U;
            }
        }
        break;

    case YMODEM_TX_DATA:
        if (ch == YMODEM_ACK)
        {
            if (!tx_send_next_data(ctx))
            {
                tx_enter_eot(ctx);
            }
            else
            {
                ctx->timer_ms    = 0U;
                ctx->retry_count = 0U;
            }
        }
        else if (ch == YMODEM_NAK)
        {
            tx_resend_current(ctx);
            ctx->timer_ms = 0U;
        }
        break;

    case YMODEM_TX_WAIT_EOT_NAK:
        if (ch == YMODEM_NAK)
        {
            put_char(ctx, YMODEM_EOT);       /* 第 2 个 EOT */
            ctx->state       = YMODEM_TX_WAIT_EOT_ACK;
            ctx->timer_ms    = 0U;
            ctx->retry_count = 0U;
        }
        else if (ch == YMODEM_ACK)
        {
            /* 少数接收端首个 EOT 直接 ACK：直接进入等待 'C' */
            ctx->state       = YMODEM_TX_WAIT_FIN_C;
            ctx->timer_ms    = 0U;
            ctx->retry_count = 0U;
        }
        break;

    case YMODEM_TX_WAIT_EOT_ACK:
        if (ch == YMODEM_ACK)
        {
            ctx->state       = YMODEM_TX_WAIT_FIN_C;
            ctx->timer_ms    = 0U;
            ctx->retry_count = 0U;
        }
        break;

    case YMODEM_TX_WAIT_FIN_C:
        if (ch == YMODEM_C)
        {
            tx_begin_or_end(ctx);
        }
        break;

    case YMODEM_TX_WAIT_FIN_ACK:
        if (ch == YMODEM_ACK)
        {
            finish(ctx, YMODEM_OK);
        }
        break;

    default:
        break;
    }
    /* 注意：不再对任意输入字节刷新静默计时，仅上述有效协议事件清零，
       使噪声无法推迟超时判定(V1 缺陷)。*/
}

/* ================================================================
 * 私有：tick 超时/重传
 * ================================================================ */
static void rx_tick(ymodem_ctx_t *ctx, uint32_t elapsed)
{
    ctx->timer_ms += elapsed;

    if (ctx->state == YMODEM_RX_WAIT_HEADER)
    {
        if (ctx->timer_ms >= YMODEM_RX_START_TIMEOUT_MS)
        {
            ctx->timer_ms = 0U;
            ctx->retry_count++;
            if (ctx->retry_count > YMODEM_RX_MAX_INIT_RETRY)
            {
                finish(ctx, YMODEM_ERR_TIMEOUT);
            }
            else
            {
                put_char(ctx, YMODEM_C); /* 重发握手字符 */
            }
        }
    }
    else /* RX_DATA / RX_WAIT_EOT2 */
    {
        if (ctx->timer_ms >= YMODEM_RX_DATA_TIMEOUT_MS)
        {
            fail(ctx, YMODEM_ERR_TIMEOUT);
        }
        else if (ctx->frame_index > 0U &&
                 ctx->timer_ms >= YMODEM_RX_FRAME_GAP_MS)
        {
            /* 帧内断流：丢弃半帧重新同步，等对端超时重发恢复；
               不再依赖“凑满后 CRC 必错”的慢速恢复路径 */
            frame_reset(ctx);
        }
    }
}

static void tx_tick(ymodem_ctx_t *ctx, uint32_t elapsed)
{
    ctx->timer_ms += elapsed;

    if (ctx->state == YMODEM_TX_WAIT_C)
    {
        if (ctx->timer_ms >= YMODEM_TX_WAIT_C_TIMEOUT_MS)
        {
            finish(ctx, YMODEM_ERR_TIMEOUT);
        }
        return;
    }

    /* TX_WAIT_HDR_C 只是等 'C'，重发无意义，超时计数放弃 */
    if (ctx->state == YMODEM_TX_WAIT_HDR_C)
    {
        if (ctx->timer_ms >= YMODEM_TX_RESP_TIMEOUT_MS)
        {
            ctx->timer_ms = 0U;
            ctx->retry_count++;
            if (ctx->retry_count > YMODEM_TX_MAX_RETRY)
            {
                fail(ctx, YMODEM_ERR_TIMEOUT);
            }
        }
        return;
    }

    /* TX_WAIT_FIN_C：所有文件均已发完，对端未再发 'C' 即视为会话正常
       结束(部分第三方接收端收完最后一个 EOT-ACK 后静默退出，V1 会把
       这种实际成功的传输误报为 YMODEM_ERR_TIMEOUT) */
    if (ctx->state == YMODEM_TX_WAIT_FIN_C)
    {
        if (ctx->timer_ms >= YMODEM_TX_RESP_TIMEOUT_MS)
        {
            finish(ctx, YMODEM_OK);
        }
        return;
    }

    /* 需要重发的等待状态 */
    if (ctx->state == YMODEM_TX_WAIT_HDR_ACK ||
        ctx->state == YMODEM_TX_DATA         ||
        ctx->state == YMODEM_TX_WAIT_EOT_NAK ||
        ctx->state == YMODEM_TX_WAIT_EOT_ACK ||
        ctx->state == YMODEM_TX_WAIT_FIN_ACK)
    {
        if (ctx->timer_ms >= YMODEM_TX_RESP_TIMEOUT_MS)
        {
            ctx->timer_ms = 0U;
            ctx->retry_count++;
            if (ctx->retry_count > YMODEM_TX_MAX_RETRY)
            {
                fail(ctx, YMODEM_ERR_TIMEOUT);
            }
            else
            {
                tx_resend_current(ctx);
            }
        }
    }
}

/* ================================================================
 * 对外 API
 * ================================================================ */
int ymodem_init(ymodem_ctx_t *ctx, ymodem_mode_t mode, const ymodem_ops_t *ops)
{
    if (ctx == NULL || ops == NULL || ops->send_char == NULL)
    {
        return -1;
    }
    if (mode != YMODEM_MODE_RECEIVER && mode != YMODEM_MODE_SENDER)
    {
        return -1;
    }
    /* 按角色校验必填回调，避免传输中途以错误的失败码终止(V1 隐患) */
    if (mode == YMODEM_MODE_RECEIVER)
    {
        if (ops->on_file_open == NULL || ops->on_write == NULL)
        {
            return -1;
        }
    }
    else
    {
        if (ops->on_tx_file_open == NULL || ops->on_tx_read == NULL)
        {
            return -1;
        }
    }

    memset(ctx, 0x00, sizeof(*ctx));
    ctx->mode  = mode;
    ctx->ops   = ops;
    ctx->state = YMODEM_STATE_IDLE;
    return 0;
}

void ymodem_start(ymodem_ctx_t *ctx)
{
    if (ctx == NULL || ctx->ops == NULL)
    {
        return;
    }

    frame_reset(ctx);
    ctx->timer_ms     = 0U;
    ctx->retry_count  = 0U;
    ctx->nak_count    = 0U;
    ctx->cancel_count = 0U;
    ctx->file_opened  = false;

    if (ctx->mode == YMODEM_MODE_RECEIVER)
    {
        ctx->expected_seq = 0U;
        ctx->state        = YMODEM_RX_WAIT_HEADER;
        put_char(ctx, YMODEM_C);   /* 启动握手 */
    }
    else if (ctx->mode == YMODEM_MODE_SENDER)
    {
        ctx->state = YMODEM_TX_WAIT_C;
    }
}

void ymodem_receive_byte(ymodem_ctx_t *ctx, uint8_t ch)
{
    if (ctx == NULL || ctx->ops == NULL)
    {
        return;
    }

    if (ctx->mode == YMODEM_MODE_RECEIVER)
    {
        rx_feed_byte(ctx, ch);
    }
    else if (ctx->mode == YMODEM_MODE_SENDER)
    {
        tx_feed_byte(ctx, ch);
    }
}

void ymodem_tick(ymodem_ctx_t *ctx, uint32_t elapsed_ms)
{
    if (ctx == NULL || ctx->ops == NULL)
    {
        return;
    }
    if (ctx->state == YMODEM_STATE_IDLE ||
        ctx->state == YMODEM_STATE_DONE ||
        ctx->state == YMODEM_STATE_ERROR)
    {
        return;
    }

    if (ctx->mode == YMODEM_MODE_RECEIVER)
    {
        rx_tick(ctx, elapsed_ms);
    }
    else if (ctx->mode == YMODEM_MODE_SENDER)
    {
        tx_tick(ctx, elapsed_ms);
    }
}

void ymodem_abort(ymodem_ctx_t *ctx)
{
    if (ctx == NULL || ctx->ops == NULL)
    {
        return;
    }
    if (ctx->state == YMODEM_STATE_IDLE ||
        ctx->state == YMODEM_STATE_DONE ||
        ctx->state == YMODEM_STATE_ERROR)
    {
        return;
    }

    send_can_abort(ctx);
    finish(ctx, YMODEM_ERR_ABORT);
}
