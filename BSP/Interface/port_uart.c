/**
 * @file port_uart.c
 * @brief UART 接口层实现
 * @note 隔离 HAL 库，屏蔽 DMA/中断实现细节；RX/TX 缓冲区由上层注入。
 *
 * 接收流程：循环 DMA + IDLE 中断 → lwrb_write → on_rx_data 回调
 * 发送流程（队列模式，tx_rb != NULL）：lwrb_write → DMA 线性段发送 → 回调后自动取下一段
 * 发送流程（直连模式，tx_rb == NULL）：HAL_UART_Transmit_DMA 直接发送，缓冲区须在回调前保持有效
 */

#include "port_uart.h"
#include "usart.h"
#include <string.h>

/* ================================================================
 * 配置宏
 * ================================================================ */

/* 超时下限，防止短帧超时为 0 */
#define UART_TX_TIMEOUT_MS_MIN   10U

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

/* 获取对应串口上下文 */
static uart_context_t *get_ctx(port_uart_id_t uart)
{
    if (uart >= PORT_UART_MAX)
        return NULL;

    if (!s_ctx[uart].initialized)
        return NULL;

    return &s_ctx[uart];
}

/* 根据波特率和数据长度计算发送超时时间,公式: 2 * 数据长度 * 每字节传输时间 */
static uint32_t calc_tx_timeout_ms(uint16_t len, uint32_t baudrate)
{
    uint32_t total_us = (uint32_t)len * UART_BYTE_TIME_US(baudrate) * 2UL;
    uint32_t total_ms = total_us / 1000UL;
    return (total_ms < UART_TX_TIMEOUT_MS_MIN) ? UART_TX_TIMEOUT_MS_MIN : total_ms;
}

/* 循环 DMA 接收通道初始化，重置软件写指针基准 */
static bsp_status_t start_rx_dma(uart_context_t *ctx)
{
    /* 循环模式利用硬件自动绕回，无需软件重启 DMA */
    ctx->huart->hdmarx->Init.Mode = DMA_CIRCULAR;
    HAL_DMA_Init(ctx->huart->hdmarx);

    HAL_StatusTypeDef ret = HAL_UART_Receive_DMA(ctx->huart, ctx->rx_dma_buf, ctx->rx_dma_size);
    if (ret != HAL_OK)
        return hal_to_bsp_status(ret);

    /* 屏蔽半满中断，减少无效触发 */
    __HAL_DMA_DISABLE_IT(ctx->huart->hdmarx, DMA_IT_HT);

    ctx->dma_write_pos = 0;
    return BSP_OK;
}

/* 从 tx_rb 取出一段连续数据发起 DMA（队列模式专用） */
static void tx_start_dma(uart_context_t *ctx)
{
    if (ctx->tx_busy)
        return;

    lwrb_sz_t len;
    /* 取线性可读段首地址，避免跨绕回点被迫拆分两次 DMA */
    const void *ptr = lwrb_get_linear_block_read_address(ctx->tx_rb);
    len = lwrb_get_linear_block_read_length(ctx->tx_rb);
    if (len == 0)
        return;

    ctx->tx_dma_len = (uint16_t)len;
    ctx->tx_busy    = true;
    HAL_UART_Transmit_DMA(ctx->huart, (uint8_t *)ptr, (uint16_t)len);
}

/* ================================================================
 * 初始化 API
 * ================================================================ */

/**
 * @brief 初始化指定串口
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

    /* 清空运行状态 */
    ctx->tx_busy      = false;
    ctx->tx_dma_len   = 0;
    ctx->one_shot_cb  = NULL;
    ctx->one_shot_ctx = NULL;

    ctx->initialized = true;

    /* 启动 DMA 接收 */
    if (start_rx_dma(ctx) != BSP_OK)
        return BSP_ERROR;

    __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
    ctx->rx_enabled = true;

    return BSP_OK;
}

/**
 * @brief 反初始化指定串口，停止 DMA，释放底层资源
 */
bsp_status_t port_uart_deinit(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    __HAL_UART_DISABLE_IT(ctx->huart, UART_IT_IDLE);
    HAL_UART_DMAStop(ctx->huart);
    HAL_UART_Abort(ctx->huart);

    ctx->initialized = false;
    ctx->rx_enabled  = false;
    ctx->tx_busy     = false;

    return BSP_OK;
}

/* ================================================================
 * 发送 API
 * ================================================================ */

/**
 * @brief 阻塞写：等待发送完成后返回（超时由波特率自动推算）
 */
bsp_status_t port_uart_write(port_uart_id_t uart, const uint8_t *data, uint16_t len)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;
    if (data == NULL || len == 0)
        return BSP_EINVAL;

    uint32_t          timeout = calc_tx_timeout_ms(len, ctx->huart->Init.BaudRate);
    HAL_StatusTypeDef ret     = HAL_UART_Transmit(ctx->huart, (uint8_t *)data, len, timeout);
    return hal_to_bsp_status(ret);
}

/**
 * @brief 异步写：提交后立即返回，完成时触发回调
 *
 * 队列模式（tx_rb != NULL）：cb / user_ctx 参数被忽略，以 init 注册的 on_tx_complete 为准。
 * 直连模式（tx_rb == NULL）：cb != NULL 时覆盖全局 on_tx_complete 仅对本次生效。
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
        /* 队列模式：写入 lwrb 后由 DMA 链式消耗 */
        uint32_t  primask = port_enter_critical();
        lwrb_sz_t written = lwrb_write(ctx->tx_rb, data, len);
        port_exit_critical(primask);

        if (written < (lwrb_sz_t)len)
            return BSP_ERROR;

        tx_start_dma(ctx);
        return BSP_OK;
    }
    else
    {
        /* 直连模式：上次未完成则拒绝新请求 */
        if (ctx->tx_busy)
            return BSP_BUSY;

        uint32_t primask  = port_enter_critical();
        ctx->one_shot_cb  = cb;
        ctx->one_shot_ctx = user_ctx;
        ctx->tx_busy      = true;
        port_exit_critical(primask);

        HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(ctx->huart, (uint8_t *)data, len);
        if (ret != HAL_OK)
        {
            ctx->tx_busy = false;
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

    bsp_status_t ret = start_rx_dma(ctx);
    if (ret == BSP_OK)
    {
        __HAL_UART_ENABLE_IT(ctx->huart, UART_IT_IDLE);
        ctx->rx_enabled = true;
    }

    return ret;
}

/**
 * @brief 停止 DMA 接收
 */
bsp_status_t port_uart_disable_rx(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    if (!ctx->rx_enabled)
        return BSP_OK;

    __HAL_UART_DISABLE_IT(ctx->huart, UART_IT_IDLE);
    HAL_UART_DMAStop(ctx->huart);
    ctx->rx_enabled = false;

    return BSP_OK;
}

/* ================================================================
 * 错误查询 API
 * ================================================================ */

/**
 * @brief 查询最近一次硬件错误标志（调用后自动清除）
 */
port_uart_error_t port_uart_get_error(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return PORT_UART_ERR_NONE;

    uint32_t          hal_err = HAL_UART_GetError(ctx->huart);
    port_uart_error_t ret     = PORT_UART_ERR_NONE;

    if (hal_err & HAL_UART_ERROR_ORE) ret |= PORT_UART_ERR_ORE;
    if (hal_err & HAL_UART_ERROR_FE)  ret |= PORT_UART_ERR_FE;
    if (hal_err & HAL_UART_ERROR_NE)  ret |= PORT_UART_ERR_NE;
    if (hal_err & HAL_UART_ERROR_PE)  ret |= PORT_UART_ERR_PE;

    if (ret != PORT_UART_ERR_NONE)
    {
        /* 清除寄存器错误标志位 */
        __HAL_UART_CLEAR_OREFLAG(ctx->huart);
        __HAL_UART_CLEAR_NEFLAG(ctx->huart);
        __HAL_UART_CLEAR_FEFLAG(ctx->huart);
        __HAL_UART_CLEAR_PEFLAG(ctx->huart);
        /* 复位 HAL 软件层错误状态机 */
        ctx->huart->ErrorCode = HAL_UART_ERROR_NONE;
    }

    return ret;
}

/**
 * @brief 尝试从错误状态恢复（重启 DMA，清除硬件错误标志）
 */
bsp_status_t port_uart_recover(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return BSP_EINVAL;

    if (HAL_UART_Abort(ctx->huart) != HAL_OK)
        return BSP_ERROR;

    __HAL_UART_CLEAR_OREFLAG(ctx->huart);
    __HAL_UART_CLEAR_NEFLAG(ctx->huart);
    __HAL_UART_CLEAR_FEFLAG(ctx->huart);
    __HAL_UART_CLEAR_PEFLAG(ctx->huart);
    ctx->huart->ErrorCode = HAL_UART_ERROR_NONE;

    ctx->tx_busy    = false;
    ctx->tx_dma_len = 0;

    if (ctx->rx_enabled)
        start_rx_dma(ctx);

    return BSP_OK;
}

/* ================================================================
 * 中断入口
 * ================================================================ */

/**
 * @brief 统一 IRQ 入口，在 USARTx_IRQHandler 中调用
 */
void port_uart_irq_handler(port_uart_id_t uart)
{
    uart_context_t *ctx = get_ctx(uart);
    if (ctx == NULL)
        return;

    if (!__HAL_UART_GET_FLAG(ctx->huart, UART_FLAG_IDLE))
        return;

    __HAL_UART_CLEAR_IDLEFLAG(ctx->huart);

    uint16_t cur_pos  = ctx->rx_dma_size - (uint16_t)ctx->huart->hdmarx->Instance->NDTR;
    uint16_t last_pos = ctx->dma_write_pos;

    // 过滤无有效数据的干扰中断
    if (cur_pos == last_pos)
        return;

    uint16_t recv_len;

    if (cur_pos > last_pos)
    {
        /* 线性段：[last_pos, cur_pos) */
        recv_len = cur_pos - last_pos;
        lwrb_write(ctx->rx_rb, &ctx->rx_dma_buf[last_pos], recv_len);
    }
    else
    {
        /* 绕回段：[last_pos, size) + [0, cur_pos) */
        uint16_t tail_len = ctx->rx_dma_size - last_pos;
        uint16_t head_len = cur_pos;
        recv_len = tail_len + head_len;
        lwrb_write(ctx->rx_rb, &ctx->rx_dma_buf[last_pos], tail_len);
        lwrb_write(ctx->rx_rb, &ctx->rx_dma_buf[0],        head_len);
    }

    ctx->dma_write_pos = cur_pos;

    if (ctx->on_rx_data != NULL)
        ctx->on_rx_data(uart, recv_len, ctx->user_ctx);
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
            /* 队列模式：释放已发送的线性段，继续消耗剩余数据 */
            lwrb_skip(ctx->tx_rb, ctx->tx_dma_len);
            ctx->tx_dma_len = 0;
            ctx->tx_busy    = false;

            if (ctx->on_tx_complete != NULL)
                ctx->on_tx_complete((uint8_t)uart, BSP_OK, ctx->user_ctx);

            tx_start_dma(ctx);
        }
        else
        {
            /* 直连模式：触发本次专属回调或全局回调 */
            ctx->tx_busy = false;

            port_async_cb_t cb  = ctx->one_shot_cb;
            void           *uc  = ctx->one_shot_ctx;
            ctx->one_shot_cb    = NULL;
            ctx->one_shot_ctx   = NULL;

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

        ctx->tx_busy    = false;
        ctx->tx_dma_len = 0;

        if (ctx->on_error != NULL)
            ctx->on_error((uint8_t)uart, BSP_ERROR, ctx->user_ctx);

        if (ctx->rx_enabled)
        {
            /* 恢复脱机的循环 DMA 接收流 */
            start_rx_dma(ctx);
        }

        break;
    }
}
