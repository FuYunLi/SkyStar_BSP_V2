/**
 * @file port_uart.c
 * @brief UART 接口层实现（V2 合并修复版）
 * @note 隔离 HAL 库，屏蔽 DMA/中断实现细节；RX/TX 缓冲区由上层注入。
 *
 * 接收流程：循环 DMA + IDLE 中断 → lwrb_write（含溢出核算）→ on_rx_data 回调
 * 发送流程（队列模式，tx_rb != NULL）：lwrb_write → DMA 线性段发送 → 回调后自动取下一段
 * 发送流程（直连模式，tx_rb == NULL）：HAL_UART_Transmit_DMA 直接发送，缓冲区须在回调前保持有效
 * 错误流程（分工式）：IRQ 入口先做 IDLE 断帧搬运，同拍的 ORE/FE/NE/PE 在清 IDLE
 *           前锁存进 ErrorCode（清 IDLE 的"读 SR+读 DR"序列会吞掉硬件位），
 *           委托 HAL_UART_IRQHandler 后由本层补位分发；
 *           非同拍错误经委托链走 HAL_UART_ErrorCallback 分流：
 *           RX 错误先排空已收数据再自愈接收流；仅 TX DMA 级错误才复位发送流并兑现失败通知
 *
 * ============================ 集成要求（缺一不可）===========================
 * 1. USARTx_IRQHandler       → 只调用 port_uart_irq_handler()。内部已驱动
 *    HAL_UART_IRQHandler——TX 完成链最后一环与错误回调都依赖它，外部不得叠加调用
 *    （叠加会导致错误双重处理）。
 * 2. DMAx_Streamx_IRQHandler → 调用 HAL_DMA_IRQHandler()。TX 完成链第一环在 DMA 中断：
 *    Normal 模式下 DMA 传输完成仅使能 UART TC 中断，TxCpltCallback 实际在
 *    USART 中断的 HAL_UART_IRQHandler 中触发，两级中断缺一不可。
 *
 * ============================ 容量规划（方案固有盲区）===========================
 * 仅靠 IDLE 搬运：一个 IDLE 窗口内 DMA 缓冲整圈绕回时软件无法区分
 * （cur_pos == last_pos 被当作"无新数据"），丢失不可检测。因此：
 *   rx_dma_buf ≥ 最长无间隙连续突发字节数；
 *   rx_rb      ≥ 2 × rx_dma_buf + 上层处理延迟窗口内可达字节数。
 * rx_rb 满导致的溢出锁存于内部 rx_overflow（本轮不暴露查询 API，见 V2 说明文档）。
 */

#include "port_uart.h"
#include "usart.h"
#include <string.h>

/* ================================================================
 * 配置宏
 * ================================================================ */

/* 超时下限，防止短帧超时为 0 */
#define UART_TX_TIMEOUT_MS_MIN   10U

/* 波特率异常（0）时的兜底超时，防除零 */
#define UART_TX_TIMEOUT_MS_SAFE  1000U

/* 单字节传输时间（us），含起止位共 10 bit */
#define UART_BYTE_TIME_US(baud)  (10000000UL / (baud))

/* ================================================================
 * 串口上下文结构（模块内私有）
 * ================================================================ */

typedef struct
{
    UART_HandleTypeDef *huart;
    port_uart_id_t      id;

    /* RX 侧 */
    uint8_t  *rx_dma_buf;
    uint16_t  rx_dma_size;
    /* IDLE 中断中上次已搬运的 DMA 写指针 */
    uint16_t  dma_write_pos;
    /* 上层注入，驱动只写不持有 */
    lwrb_t   *rx_rb;
    /* 丢数锁存：rx_rb 满截断 或 ORE 丢字节时置位，init/recover 清除。
     * 预留后续以读清型 API 挂出（届时才需动 port_uart.h） */
    volatile bool rx_overflow;

    /* TX 侧 */
    /* 上层注入，NULL = 直连模式 */
    lwrb_t           *tx_rb;
    volatile bool     tx_busy;
    /* 当前 DMA 正在发送的连续段长度（队列模式用） */
    volatile uint16_t tx_dma_len;

    /* 全局回调（init 时注册） */
    port_async_cb_t on_tx_complete;
    port_async_cb_t on_error;
    void (*on_rx_data)(port_uart_id_t uart, uint16_t len, void *user_ctx);
    void *user_ctx;

    /* 直连模式下本次 write_async 的一次性回调，可覆盖全局 on_tx_complete */
    port_async_cb_t  one_shot_cb;
    void            *one_shot_ctx;

    /* 最近错误位快照（ISR 置位 / get_error 读后清）。
     * 与 huart->ErrorCode 解耦：错误分发完成后即清 HAL 层错误码，
     * 防历史位累积让后续纯 RX 错误被误判为 DMA 错误误杀发送流 */
    volatile uint32_t last_error;

    volatile bool initialized;
    volatile bool rx_enabled;
} uart_context_t;

/* ================================================================
 * 模块内部数据
 * ================================================================ */

static UART_HandleTypeDef *s_uart_map[PORT_UART_MAX] =
{
    [PORT_UART_1] = &huart1,
};

static uart_context_t s_ctx[PORT_UART_MAX];

/* ================================================================
 * 内部工具函数
 * ================================================================ */

/* 获取对应串口上下文（IRQ 入口不使用此函数：未 init 时也要能清标志防风暴） */
static uart_context_t *get_ctx(port_uart_id_t uart)
{
    if (uart >= PORT_UART_MAX)
        return NULL;

    if (!s_ctx[uart].initialized)
        return NULL;

    return &s_ctx[uart];
}

/* DMA 剩余计数值读取。平台相关读取点收进一个函数，移植时只改这里 */
static inline uint16_t rx_remaining(const uart_context_t *ctx)
{
    return (uint16_t)__HAL_DMA_GET_COUNTER(ctx->huart->hdmarx);
}

/* 根据波特率和数据长度计算发送超时时间,公式: 2 * 数据长度 * 每字节传输时间
 * 中间量用 64 位，防止低波特率大帧时 uint32 溢出（300bps × 64KB ≈ 4.4×10⁹） */
static uint32_t calc_tx_timeout_ms(uint16_t len, uint32_t baudrate)
{
    if (baudrate == 0)
        return UART_TX_TIMEOUT_MS_SAFE;

    uint64_t total_us = (uint64_t)len * UART_BYTE_TIME_US(baudrate) * 2UL;
    uint32_t total_ms = (uint32_t)(total_us / 1000ULL);
    return (total_ms < UART_TX_TIMEOUT_MS_MIN) ? UART_TX_TIMEOUT_MS_MIN : total_ms;
}

/* 循环 DMA 接收通道启动，重置软件写指针基准。
 * 仅允许在 RX 流确已停止时调用（init / enable_rx / recover / 错误自愈） */
static bsp_status_t s_rx_start_dma(uart_context_t *ctx)
{
    if (ctx->huart->hdmarx == NULL)
        return BSP_ENODEV;

    /* 防呆：旧流仍在跑（RxState = BUSY_RX）时强启只会得到 HAL_BUSY，
     * 且复位 dma_write_pos 会使软件基准与硬件写指针脱钩，拒绝启动保持旧流 */
    if (ctx->huart->RxState != HAL_UART_STATE_READY)
        return BSP_BUSY;

    /* 指针基准复位、残留标志清理与 DMA 重启动须对 IDLE 中断保持原子：
     * 旧基准已作废，残留 IDLE 标志若在窗口内触发，ISR 会按错误基准搬运；
     * CLEAR_IDLEFLAG 的"读 SR + 读 DR"序列顺带清尽 ORE/FE/NE/PE */
    uint32_t primask = port_enter_critical();

    __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);
    ctx->dma_write_pos = 0;

    ctx->huart->hdmarx->Init.Mode = DMA_CIRCULAR;
    HAL_StatusTypeDef init_ret = HAL_DMA_Init(ctx->huart->hdmarx);

    HAL_StatusTypeDef ret = (init_ret == HAL_OK)
        ? HAL_UART_Receive_DMA(ctx->huart, ctx->rx_dma_buf, ctx->rx_dma_size)
        : HAL_ERROR;

    port_exit_critical(primask);

    if (ret != HAL_OK)
        return hal_to_bsp_status(ret);

    /* 屏蔽半满中断，减少无效触发 */
    __HAL_DMA_DISABLE_IT(ctx->huart->hdmarx, DMA_IT_HT);

    /* 显式补开错误中断源：现代 F4 HAL 的 Receive_DMA 已 SET_BIT(EIE)（幂等），
     * 老版本不开——保证错误链路在任何 HAL 版本下即时触发而非拖到下次 IDLE */
    __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_ERR);

    return BSP_OK;
}

/* 把 DMA 缓冲中 [dma_write_pos, 硬件写位置) 的已收数据搬进 rx_rb 并回调上报。
 * IDLE 断帧与错误自愈排空共用：两者都保证搬运期间硬件写指针不再推进——
 * 前者线路已空闲一帧时间，后者 RX 流已被中止（NDTR 冻结）。
 * 无新数据（含整圈绕回盲区）时安静返回 0、不回调。 */
static void s_rx_drain_dma_buf(uart_context_t *ctx)
{
    uint16_t cur_pos  = ctx->rx_dma_size - rx_remaining(ctx);
    uint16_t last_pos = ctx->dma_write_pos;

    /* NDTR 瞬态为 0 = 刚写满整圈重载，等价位置 0 */
    if (cur_pos >= ctx->rx_dma_size)
        cur_pos = 0;

    // 过滤无有效数据的干扰中断（亦可能是整圈绕回，无法区分，见文件头容量规划）
    if (cur_pos == last_pos)
        return;

    uint16_t recv_len      = 0;
    uint16_t written_total = 0;

    if (cur_pos > last_pos)
    {
        /* 线性段：[last_pos, cur_pos) */
        recv_len      = cur_pos - last_pos;
        written_total = (uint16_t)lwrb_write(ctx->rx_rb, &ctx->rx_dma_buf[last_pos],
                                             recv_len);
    }
    else
    {
        /* 绕回段：[last_pos, size) + [0, cur_pos)；
         * 尾段未足额写入（rx_rb 满）时头段整体丢弃，不再写入半个垃圾段 */
        uint16_t tail_len = ctx->rx_dma_size - last_pos;
        uint16_t head_len = cur_pos;
        recv_len = tail_len + head_len;

        written_total = (uint16_t)lwrb_write(ctx->rx_rb, &ctx->rx_dma_buf[last_pos],
                                             tail_len);
        if (written_total == tail_len)
            written_total += (uint16_t)lwrb_write(ctx->rx_rb, &ctx->rx_dma_buf[0],
                                                  head_len);
    }

    ctx->dma_write_pos = cur_pos;

    if (written_total < recv_len)
        ctx->rx_overflow = true;        /* rx_rb 容量不足，溢出部分丢弃并锁存 */

    /* len = 实际写入 rx_rb 的字节数，上层按此解析不会错位 */
    if (ctx->on_rx_data != NULL)
        ctx->on_rx_data(ctx->id, written_total, ctx->user_ctx);
}

/* 从 tx_rb 取出一段连续数据发起 DMA（队列模式专用）。
 * 主线程（write_async）与 ISR（TxCpltCallback）双上下文调用：
 * 检查-置位-启动整体原子，启动失败回滚状态（数据留在 rb，下次提交自动带出） */
static void s_tx_start_dma(uart_context_t *ctx)
{
    if (ctx->tx_rb == NULL)
        return;

    uint32_t primask = port_enter_critical();

    if (ctx->tx_busy)
    {
        port_exit_critical(primask);
        return;
    }

    /* 取线性可读段首地址，避免跨绕回点被迫拆分两次 DMA */
    lwrb_sz_t   len = lwrb_get_linear_block_read_length(ctx->tx_rb);
    const void *ptr = lwrb_get_linear_block_read_address(ctx->tx_rb);
    if (len == 0)
    {
        port_exit_critical(primask);
        return;
    }
    if (len > 0xFFFFU)                 /* uint16_t 截断保护（DMA 长度寄存器亦 16bit） */
        len = 0xFFFFU;

    ctx->tx_dma_len = (uint16_t)len;
    ctx->tx_busy    = true;

    /* 临界区内仅做寄存器配置，微秒级，ISR 上下文嵌套调用可接受 */
    if (HAL_UART_Transmit_DMA(ctx->huart, (uint8_t *)ptr, (uint16_t)len) != HAL_OK)
    {
        /* 启动失败（HAL_BUSY/ERROR）：回滚状态，数据留在 rb 等待重试 */
        ctx->tx_dma_len = 0;
        ctx->tx_busy    = false;
    }

    port_exit_critical(primask);
}

/* 错误分发（仅 ISR 上下文，err 为 HAL_UART_ERROR_xxx 位组合）。
 * 语义决策：
 *  - 纯 RX 错误（FE/NE/PE/ORE）一律不动 TX 状态——接收坏数据不停 TX DMA，
 *    误清 tx_dma_len 会使 TxCplt 里 lwrb_skip(0) 不释放已发段，队列错位重发；
 *  - 仅 TX DMA 级错误才复位发送流并清空 tx_rb（传输已不可信，半截流续发
 *    无价值），在途发送以 BSP_ERROR 兑现通知，上层据此重发；
 *  - 处理完成后立即消费并清零 HAL 层错误码（HAL 在 ErrorCallback 之后不再
 *    读取），杜绝历史位累积误判 */
static void s_error_dispatch(uart_context_t *ctx, uint32_t err)
{
    UART_HandleTypeDef *huart = ctx->huart;

    bool uart_err    = (err & (HAL_UART_ERROR_ORE | HAL_UART_ERROR_FE |
                               HAL_UART_ERROR_NE  | HAL_UART_ERROR_PE)) != 0U;
    bool dma_err     = (err & HAL_UART_ERROR_DMA) != 0U;
    bool rx_dma_dead = dma_err && huart->hdmarx != NULL &&
                       huart->hdmarx->ErrorCode != HAL_DMA_ERROR_NONE;
    bool tx_dma_dead = dma_err && huart->hdmatx != NULL &&
                       huart->hdmatx->ErrorCode != HAL_DMA_ERROR_NONE;

    /* 错误位代入内部锁存后，HAL/DMA 层错误码就地清零（快照已消费完毕） */
    ctx->last_error |= err;
    huart->ErrorCode = HAL_UART_ERROR_NONE;
    if (huart->hdmarx != NULL)
        huart->hdmarx->ErrorCode = HAL_DMA_ERROR_NONE;
    if (huart->hdmatx != NULL)
        huart->hdmatx->ErrorCode = HAL_DMA_ERROR_NONE;

    if (err & HAL_UART_ERROR_ORE)
        ctx->rx_overflow = true;       /* ORE 丢字节，锁存供溢出感知 */

    /* ---- RX 侧自愈：仅在 HAL 已停掉接收流时重启。
     * DMA 模式下 ORE/FE/NE 被视为阻塞错误，HAL 已 EndRxTransfer（RxState=READY）；
     * PE 为非阻塞错误，流仍在跑（BUSY_RX），此时重启反而会破坏基准 ---- */
    if ((uart_err || rx_dma_dead) && ctx->rx_enabled &&
        huart->RxState != HAL_UART_STATE_BUSY_RX)
    {
        /* 流已被 HAL/DMA 中止（NDTR 冻结）：先按旧基准排空已收数据再重启，
         * 错误前的正常字节（FE/NE 仅是瑕疵，字节已在缓冲）不随基准归零丢失。
         * IDLE 同拍场景流仍活着（RxState = BUSY_RX），不会进到这里重复搬运 */
        s_rx_drain_dma_buf(ctx);

        if (s_rx_start_dma(ctx) == BSP_OK)
            __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);  /* 防 HAL 中止路径连带关 IDLE */
    }

    /* ---- TX 侧：仅 TX DMA 错误且确有在途发送才复位，接收侧坏数据不误伤发送流 ---- */
    if (tx_dma_dead && ctx->tx_busy)
    {
        uint32_t primask = port_enter_critical();
        ctx->tx_busy     = false;
        ctx->tx_dma_len  = 0;
        if (ctx->tx_rb != NULL)
            lwrb_reset(ctx->tx_rb);    /* 在途段的 skip 不会再来，不清则旧数据重发 */
        port_exit_critical(primask);

        /* 在途发送以失败兑现，不静默吞掉 */
        port_async_cb_t cb = ctx->one_shot_cb;
        void           *uc = ctx->one_shot_ctx;
        ctx->one_shot_cb   = NULL;
        ctx->one_shot_ctx  = NULL;

        if (cb != NULL)
            cb((uint8_t)ctx->id, BSP_ERROR, uc);
        else if (ctx->on_tx_complete != NULL)
            ctx->on_tx_complete((uint8_t)ctx->id, BSP_ERROR, ctx->user_ctx);
    }

    if ((uart_err || dma_err) && ctx->on_error != NULL)
        ctx->on_error((uint8_t)ctx->id, BSP_ERROR, ctx->user_ctx);
}

/* ================================================================
 * 初始化 API
 * ================================================================ */

/**
 * @brief 初始化指定串口
 *
 * 重入安全：已 init 的串口再次 init = deinit + init，环形缓冲按新会话 reset。
 *
 * 示例（队列模式）：
 *   static uint8_t dma_buf[64], rb_buf[512], tx_buf[1024];
 *   static lwrb_t rx_rb, tx_rb;
 *   lwrb_init(&rx_rb, rb_buf, sizeof(rb_buf));
 *   lwrb_init(&tx_rb, tx_buf, sizeof(tx_buf));
 *   port_uart_config_t cfg = {
 *       .rx_dma_buf = dma_buf, .rx_dma_buf_size = sizeof(dma_buf),
 *       .rx_rb = &rx_rb, .tx_rb = &tx_rb, .on_rx_data = my_rx_cb
 *   };
 *   port_uart_init(PORT_UART_1, &cfg);
 */
bsp_status_t port_uart_init(port_uart_id_t uart, const port_uart_config_t *cfg)
{
    if (uart >= PORT_UART_MAX || cfg == NULL)
        return BSP_EINVAL;
    if (cfg->rx_dma_buf == NULL || cfg->rx_dma_buf_size == 0 || cfg->rx_rb == NULL)
        return BSP_EINVAL;

    uart_context_t *ctx = &s_ctx[uart];

    ctx->huart = s_uart_map[uart];
    ctx->id    = uart;

    if (ctx->huart == NULL || HAL_UART_GetState(ctx->huart) == HAL_UART_STATE_RESET)
        return BSP_ERROR;
    if (ctx->huart->hdmarx == NULL)
        return BSP_ENODEV;              /* CubeMX 忘开 RX DMA：明确报错而非 HardFault */

    /* 重入保护：先停掉上一次的 DMA 与中断，防旧流导致 Receive_DMA 返回 BUSY 半途而废 */
    if (ctx->initialized)
        port_uart_deinit(uart);

    /* 应用波特率（0 = 沿用 CubeMX 默认） */
    if (cfg->baudrate != 0)
    {
        ctx->huart->Init.BaudRate = cfg->baudrate;
        if (HAL_UART_Init(ctx->huart) != HAL_OK)
            return BSP_ERROR;
    }

    /* 绑定缓冲区 */
    ctx->rx_dma_buf  = cfg->rx_dma_buf;
    ctx->rx_dma_size = cfg->rx_dma_buf_size;
    ctx->rx_rb       = cfg->rx_rb;
    ctx->tx_rb       = cfg->tx_rb;

    /* 注册回调 */
    ctx->on_tx_complete = cfg->on_tx_complete;
    ctx->on_error       = cfg->on_error;
    ctx->on_rx_data     = cfg->on_rx_data;
    ctx->user_ctx       = cfg->user_ctx;

    /* 清空运行状态。缓冲区属新会话，统一 reset 清掉残留旧数据 */
    ctx->tx_busy      = false;
    ctx->tx_dma_len   = 0;
    ctx->one_shot_cb  = NULL;
    ctx->one_shot_ctx = NULL;
    ctx->rx_overflow  = false;
    ctx->last_error   = 0;
    lwrb_reset(ctx->rx_rb);
    if (ctx->tx_rb != NULL)
        lwrb_reset(ctx->tx_rb);

    ctx->initialized = true;

    /* 启动 DMA 接收，失败全量回滚，不留半初始化状态 */
    bsp_status_t ret = s_rx_start_dma(ctx);
    if (ret != BSP_OK)
    {
        ctx->initialized = false;
        return ret;
    }

    __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
    ctx->rx_enabled = true;

    return BSP_OK;
}

/**
 * @brief 反初始化指定串口，停止 DMA，释放底层资源
 * @note 收尾语义：在途发送被放弃；直连模式的一次性回调以 BSP_ERROR 兑现，
 *       不静默吞掉；先落状态再停硬件，让停机过程中可能触发的 HAL 回调空转。
 */
bsp_status_t port_uart_deinit(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    port_async_cb_t cb = ctx->one_shot_cb;
    void           *uc = ctx->one_shot_ctx;

    __HAL_UART_DISABLE_IT(ctx->huart, UART_IT_IDLE);
    /* TC 一并显式关闭：若 TX DMA 已在 UART_DMATransmitCplt 中置起 TCIE 后才被停，
     * TCIE 无人关会留下停机版中断风暴（HAL_UART_Abort 内部也会清，此处显式化不依赖副作用） */
    __HAL_UART_DISABLE_IT(ctx->huart, UART_IT_TC);

    uint32_t primask  = port_enter_critical();
    ctx->initialized  = false;
    ctx->rx_enabled   = false;
    ctx->tx_busy      = false;
    ctx->tx_dma_len   = 0;
    ctx->one_shot_cb  = NULL;
    ctx->one_shot_ctx = NULL;
    port_exit_critical(primask);

    /* 全通道中止：TX/RX 在途 DMA 立即弃（deinit 语义即放弃），HAL 状态机归 READY，
     * 为重入 init 扫清 BUSY 隐患 */
    HAL_UART_Abort(ctx->huart);

    if (cb != NULL)
        cb((uint8_t)uart, BSP_ERROR, uc);

    return BSP_OK;
}

/* ================================================================
 * 发送 API
 * ================================================================ */

/**
 * @brief 阻塞写：等待发送完成后返回（超时由波特率自动推算）
 * @note 若异步发送队列非空，先等待其排空再轮询发送，兑现"阻塞写"语义。
 *       排空超时按"队列存量 + 在飞段 + 本帧"合并估算，避免队列积压时
 *       按单帧超时提前 ETIMEOUT。
 */
bsp_status_t port_uart_write(port_uart_id_t uart, const uint8_t *data, uint16_t len)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;
    if (data == NULL || len == 0)
        return BSP_EINVAL;

    uint32_t baud = ctx->huart->Init.BaudRate;

    uint32_t drain_timeout = calc_tx_timeout_ms(len, baud);
    if (ctx->tx_rb != NULL)
    {
        uint32_t queued = lwrb_get_full(ctx->tx_rb) + ctx->tx_dma_len;
        drain_timeout = calc_tx_timeout_ms(
            (uint16_t)MIN(queued + (uint32_t)len, 0xFFFFU), baud);
    }

    bsp_status_t ret = port_uart_tx_wait(uart, drain_timeout);
    if (ret != BSP_OK)
        return ret;

    return hal_to_bsp_status(
        HAL_UART_Transmit(ctx->huart, (uint8_t *)data, len, calc_tx_timeout_ms(len, baud)));
}

/**
 * @brief 异步写：提交后立即返回，完成时触发回调
 *
 * 队列模式（tx_rb != NULL）：cb / user_ctx 参数被忽略，以 init 注册的 on_tx_complete 为准；
 *                            容量不足时整帧拒收返回 BSP_ENOMEM，线上不产生半帧。
 * 直连模式（tx_rb == NULL）：cb != NULL 时覆盖全局 on_tx_complete 仅对本次生效；
 *                            上次未完成时返回 BSP_BUSY。
 */
bsp_status_t port_uart_write_async(port_uart_id_t uart, const uint8_t *data, uint16_t len,
                                    port_async_cb_t cb, void *user_ctx)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;
    if (data == NULL || len == 0)
        return BSP_EINVAL;

    if (ctx->tx_rb != NULL)
    {
        /* 队列模式：先确认容量足额，整帧入队或整帧拒绝。
         * lwrb_write 是截断语义，直接写会产生"半帧入队却被发出"的幽灵半帧；
         * 也不能用 lwrb_skip 回滚——skip 动的是读指针（消费者侧资产），
         * SPSC 契约里生产者无权操作 */
        uint32_t primask = port_enter_critical();

        if (lwrb_get_free(ctx->tx_rb) < (lwrb_sz_t)len)
        {
            port_exit_critical(primask);
            return BSP_ENOMEM;
        }

        (void)lwrb_write(ctx->tx_rb, data, len);
        port_exit_critical(primask);

        s_tx_start_dma(ctx);
        return BSP_OK;                  /* BSP_OK = 已入队；泵启动失败由下次提交自动带出 */
    }
    else
    {
        /* 直连模式：检查-置位原子化，上次未完成则拒绝新请求 */
        uint32_t primask = port_enter_critical();

        if (ctx->tx_busy)
        {
            port_exit_critical(primask);
            return BSP_BUSY;
        }

        ctx->one_shot_cb  = cb;
        ctx->one_shot_ctx = user_ctx;
        ctx->tx_busy      = true;
        port_exit_critical(primask);

        HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(ctx->huart, (uint8_t *)data, len);
        if (ret != HAL_OK)
        {
            /* 失败回滚须连一次性回调一并清除，否则残留回调会被下次发送错误触发 */
            uint32_t primask2  = port_enter_critical();
            ctx->tx_busy       = false;
            ctx->one_shot_cb   = NULL;
            ctx->one_shot_ctx  = NULL;
            port_exit_critical(primask2);
            return hal_to_bsp_status(ret);
        }

        return BSP_OK;
    }
}

/**
 * @brief 查询发送通道是否忙碌
 */
bool port_uart_is_tx_busy(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return false;

    return ctx->tx_busy;
}

/**
 * @brief 阻塞等待当前异步发送完成
 */
bsp_status_t port_uart_tx_wait(port_uart_id_t uart, uint32_t timeout_ms)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    uint32_t start = HAL_GetTick();

    while (ctx->tx_busy)
    {
        if (timeout_ms != 0 && (HAL_GetTick() - start) >= timeout_ms)
            return BSP_ETIMEOUT;
    }

    return BSP_OK;
}

/* ================================================================
 * 接收控制 API
 * ================================================================ */

/**
 * @brief 启动 DMA 接收
 */
bsp_status_t port_uart_enable_rx(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    if (ctx->rx_enabled)
        return BSP_OK;

    bsp_status_t ret = s_rx_start_dma(ctx);
    if (ret == BSP_OK)
    {
        __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
        ctx->rx_enabled = true;
    }

    return ret;
}

/**
 * @brief 停止 DMA 接收
 * @note 用 AbortReceive 只停接收，不动发送流：DMAStop 会连在飞 TX DMA
 *       一起停且 TxCplt 不会触发，导致 tx_busy 永真、队列冻结。
 *       停止时刻 DMA 缓冲中未过 IDLE 边界的尾部字节会丢弃，
 *       上层应先消费完 rx_rb 再 disable。
 */
bsp_status_t port_uart_disable_rx(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    if (!ctx->rx_enabled)
        return BSP_OK;

    __HAL_UART_DISABLE_IT(ctx->huart, UART_IT_IDLE);
    HAL_UART_AbortReceive(ctx->huart);  /* 只中止接收：停 RX DMA + RxState 归 READY */
    ctx->rx_enabled = false;

    return BSP_OK;
}

/* ================================================================
 * 错误查询 API
 * ================================================================ */

/**
 * @brief 查询最近一次硬件错误标志（调用后自动清除）
 * @note 数据源为内部锁存 last_error 与 HAL ErrorCode 的并集：
 *       错误分发完成后 HAL 层错误码即被清零，锁存保证 on_error 之后仍可读到。
 */
port_uart_error_t port_uart_get_error(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return PORT_UART_ERR_NONE;

    uint32_t          hal_err = ctx->last_error | ctx->huart->ErrorCode;
    port_uart_error_t ret     = PORT_UART_ERR_NONE;

    if (hal_err & HAL_UART_ERROR_ORE) ret |= PORT_UART_ERR_ORE;
    if (hal_err & HAL_UART_ERROR_FE)  ret |= PORT_UART_ERR_FE;
    if (hal_err & HAL_UART_ERROR_NE)  ret |= PORT_UART_ERR_NE;
    if (hal_err & HAL_UART_ERROR_PE)  ret |= PORT_UART_ERR_PE;

    if (ret != PORT_UART_ERR_NONE)
    {
        /* F4：读 SR + 读 DR 的序列同时清除 ORE/NE/FE/PE，一条即可 */
        __HAL_UART_CLEAR_PEFLAG(ctx->huart);
        ctx->last_error       = 0;
        ctx->huart->ErrorCode = HAL_UART_ERROR_NONE;
    }

    return ret;
}

/**
 * @brief 尝试从错误状态恢复（重启 DMA，清除硬件错误标志）
 * @note 恢复语义：
 *  - RX：AbortReceive 复位 HAL 状态机后重启循环 DMA，错误锁存一并清除；
 *  - TX 队列模式：在飞则强制中止并整体丢弃未发队列（半截流续发无价值），
 *    有丢弃时以 BSP_ERROR 兑现 on_tx_complete；队列空闲但有积压则直接补泵；
 *  - TX 直连模式：在飞传输不强杀——缓冲所有权在上层，中止会留半截线上数据；
 *    真正坏死（TX DMA 错误）的场景已由 s_error_dispatch 复位 tx_busy 并兑现
 *    失败通知，此处保持等待自然收尾。
 */
bsp_status_t port_uart_recover(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    /* ---- RX 侧复位重启 ---- */
    HAL_UART_AbortReceive(ctx->huart);

    /* 一次读 SR + 读 DR 清尽 ORE/FE/NE/PE 硬件标志 */
    __HAL_UART_CLEAR_PEFLAG(ctx->huart);
    ctx->huart->ErrorCode = HAL_UART_ERROR_NONE;
    ctx->last_error       = 0;
    ctx->rx_overflow      = false;
    if (ctx->huart->hdmarx != NULL)
        ctx->huart->hdmarx->ErrorCode = HAL_DMA_ERROR_NONE;
    if (ctx->huart->hdmatx != NULL)
        ctx->huart->hdmatx->ErrorCode = HAL_DMA_ERROR_NONE;

    if (ctx->rx_enabled)
    {
        bsp_status_t ret = s_rx_start_dma(ctx);
        if (ret != BSP_OK)
            return ret;
        __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
    }

    /* ---- TX 侧恢复 ---- */
    if (ctx->tx_rb != NULL)
    {
        if (ctx->tx_busy)
        {
            HAL_UART_AbortTransmit(ctx->huart);

            uint32_t primask = port_enter_critical();
            ctx->tx_busy     = false;
            ctx->tx_dma_len  = 0;
            port_exit_critical(primask);

            lwrb_reset(ctx->tx_rb);     /* 在途段的 skip 不会再来，清空防旧数据重发 */

            if (ctx->on_tx_complete != NULL)
                ctx->on_tx_complete((uint8_t)uart, BSP_ERROR, ctx->user_ctx);
        }

        /* 队列空闲但有积压（此前泵启动失败过）：恢复后自动续流
         * （tx_busy 在飞时 s_tx_start_dma 内部自锁，不会双启） */
        s_tx_start_dma(ctx);
    }
    /* 直连模式（tx_rb == NULL）在飞传输不动：见函数头 @note */

    return BSP_OK;
}

/* ================================================================
 * 中断入口
 * ================================================================ */

/**
 * @brief 统一 IRQ 入口，在 USARTx_IRQHandler 中调用（不要再叠加 HAL_UART_IRQHandler）
 *
 * 分工式处理（先搬运后委托）：
 *  1. IDLE 断帧自处理——若先委托，IDLE 与错误同拍时 HAL 错误分支会先
 *     EndRxTransfer 并触发自愈重启（清 IDLE 标志、归零搬运基准），随后的
 *     搬运被跳过、同拍帧整段丢失；先搬后托则数据完整、流不被中止。
 *  2. 清 IDLE 的"读 SR + 读 DR"序列会顺带吞掉同拍的 ORE/FE/NE/PE——硬件位
 *     清掉后 HAL 将看不到，故先锁存进 ErrorCode，委托后由本函数补位分发
 *     （err_consumed），保证错误恰好上报一次。
 *  3. 委托 HAL_UART_IRQHandler：TX 完成链最后一环（清 TC、关 TCIE、
 *     TxCpltCallback）与未被消费的错误位（EIE 链路 → ErrorCallback → 分流）。
 */
void port_uart_irq_handler(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);

    if (ctx == NULL)
    {
        /* 未初始化（含已 deinit）兜底：仍清 IDLE 标志，防止中断风暴；
         * 该序列（读 SR + 读 DR）顺带清尽 ORE/FE/NE/PE */
        if (uart < PORT_UART_MAX && s_uart_map[uart] != NULL)
            __HAL_UART_CLEAR_IDLEFLAG(s_uart_map[uart]);
        return;
    }

    uint32_t sr  = READ_REG(ctx->huart->Instance->SR);
    uint32_t err = sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE);
    bool     err_consumed = false;

    if ((sr & USART_SR_IDLE) != 0U)
    {
        /* 清 IDLE 前先锁存同拍错误位（F4 上 HAL_UART_ERROR_xxx 与 SR 位同值），
         * 否则紧随的 SR+DR 读序列会把硬件位吞掉、HAL 与本层都无从得知 */
        if (err != 0U)
        {
            ctx->huart->ErrorCode |= err;
            if ((err & USART_SR_ORE) != 0U)
                ctx->rx_overflow = true;    /* ORE 丢字节，锁存供溢出感知 */
            err_consumed = true;
        }

        __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);

        /* 搬运不检查 rx_enabled：disable_rx 与挂起中断的竞态窗口内，
         * RX 流已被 Abort（NDTR 冻结），把残余数据搬进 rb 只赚不赔 */
        s_rx_drain_dma_buf(ctx);
    }

    HAL_UART_IRQHandler(ctx->huart);

    /* IDLE 同拍错误已被本层消费（硬件位清、ErrorCode 锁存），HAL 看不到，
     * 分发由此补位；非同拍错误已由委托链的 ErrorCallback 分发，不会重复 */
    if (err_consumed)
        s_error_dispatch(ctx, err);
}

/* ================================================================
 * HAL 回调函数
 * ================================================================ */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    for (port_uart_id_t uart = PORT_UART_1; uart < PORT_UART_MAX; uart++)
    {
        uart_context_t *ctx = &s_ctx[uart];

        if (ctx->huart != huart || !ctx->initialized)
            continue;

        if (ctx->tx_rb != NULL)
        {
            /* 队列模式：释放已发送的线性段，继续消耗剩余数据。
             * skip 量与状态复位在临界区内原子完成，防并发 write_async
             * 在窗口内以新段覆盖 tx_dma_len */
            uint32_t primask = port_enter_critical();
            uint16_t sent    = ctx->tx_dma_len;
            ctx->tx_dma_len  = 0;
            ctx->tx_busy     = false;
            lwrb_skip(ctx->tx_rb, sent);
            port_exit_critical(primask);

            if (ctx->on_tx_complete != NULL)
                ctx->on_tx_complete((uint8_t)uart, BSP_OK, ctx->user_ctx);

            s_tx_start_dma(ctx);
        }
        else
        {
            /* 直连模式：临界区内原子摘取一次性回调，防与 write_async 失败回滚互踩 */
            uint32_t primask = port_enter_critical();
            ctx->tx_busy     = false;

            port_async_cb_t cb = ctx->one_shot_cb;
            void           *uc = ctx->one_shot_ctx;
            ctx->one_shot_cb   = NULL;
            ctx->one_shot_ctx  = NULL;
            port_exit_critical(primask);

            if (cb != NULL)
                cb((uint8_t)uart, BSP_OK, uc);
            else if (ctx->on_tx_complete != NULL)
                ctx->on_tx_complete((uint8_t)uart, BSP_OK, ctx->user_ctx);
        }

        break;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (port_uart_id_t uart = PORT_UART_1; uart < PORT_UART_MAX; uart++)
    {
        uart_context_t *ctx = &s_ctx[uart];

        if (ctx->huart != huart || !ctx->initialized)
            continue;

        s_error_dispatch(ctx, huart->ErrorCode);

        break;
    }
}
