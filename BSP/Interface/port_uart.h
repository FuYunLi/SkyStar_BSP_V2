/**
 * @file port_uart.h
 * @brief UART 接口层头文件
 * @note 隔离 HAL 库，屏蔽 DMA/中断实现细节，对上提供统一的 UART 访问接口。
 */

#ifndef PORT_UART_H
#define PORT_UART_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_board.h"
#include "port_critical.h"
#include "lwrb/lwrb.h"

/* ================================================================
 * 串口逻辑 ID
 * ================================================================ */

typedef enum
{
    /* 调试 / Shell 串口 */
    PORT_UART_1 = 0,
    
    PORT_UART_MAX
} port_uart_id_t;

/* ================================================================
 * 错误标志（可按位或组合）
 * ================================================================ */

typedef enum
{
    /* 无错误 */
    PORT_UART_ERR_NONE = 0x00U,
    /* 溢出错误 */
    PORT_UART_ERR_ORE  = 0x01U,
    /* 帧错误 */
    PORT_UART_ERR_FE   = 0x02U,
    /* 噪声错误 */
    PORT_UART_ERR_NE   = 0x04U,
    /* 奇偶校验错 */
    PORT_UART_ERR_PE   = 0x08U,
} port_uart_error_t;

/* ================================================================
 * 初始化配置（缓冲区由调用方静态分配后注入）
 * ================================================================ */

typedef struct
{
    /* 波特率，0 表示沿用 CubeMX 默认配置，不重新设置 */
    uint32_t baudrate;

    /* DMA 接收原始缓冲区：驱动将其配置为 DMA 目标地址（循环 DMA 模式） */
    uint8_t *rx_dma_buf;
    uint16_t rx_dma_buf_size;

    /* lwrb 接收环形缓冲区：IDLE 中断触发后驱动将 DMA 数据拷入此 rb */
    /* 上层持有句柄，直接调用 lwrb_read() / lwrb_get_linear_block_read_address() 消费 */
    lwrb_t *rx_rb;

    /* lwrb 发送环形缓冲区（可选）：
     *   != NULL → 队列模式，write_async 写入后立即返回，DMA 自动链式消耗；
     *   == NULL → 直连模式，write_async 直接提交 DMA，缓冲区须在回调前保持有效。 */
    lwrb_t *tx_rb;

    /* 发送完成回调，NULL 表示不通知 */
    /* bus_id 填充为对应的 port_uart_id_t 值，result 为 BSP_OK / BSP_ERR */
    port_async_cb_t on_tx_complete;

    /* 硬件错误回调，NULL 表示不通知 */
    port_async_cb_t on_error;

    /* 接收到新数据回调（IDLE 中断搬运完成后触发），NULL 表示纯轮询模式 */
    /* len 为本次新增字节数，上层再调用 lwrb API 消费 */
    void (*on_rx_data)(port_uart_id_t uart, uint16_t len, void *user_ctx);

    /* 透明指针，所有回调均原样返回 */
    void *user_ctx;
} port_uart_config_t;

/* ================================================================
 * 初始化 API
 * ================================================================ */

/**
 * @brief 初始化指定串口
 * @param uart 目标串口 ID
 * @param cfg  配置指针，cfg->rx_dma_buf 与 cfg->rx_rb 须在调用前完成分配
 *
 * 示例（队列模式）：
 *   static uint8_t dma_buf[64], rb_buf[512], tx_buf[1024];
 *   static lwrb_t rx_rb, tx_rb;
 *   lwrb_init(&rx_rb, rb_buf, sizeof(rb_buf));
 *   lwrb_init(&tx_rb, tx_buf, sizeof(tx_buf));
 *   uart_config_t cfg = {
 *       .baudrate = 0,
 *       .rx_dma_buf = dma_buf, .rx_dma_buf_size = sizeof(dma_buf),
 *       .rx_rb = &rx_rb, .tx_rb = &tx_rb,
 *       .on_rx_data = my_rx_cb
 *   };
 *   port_uart_init(PORT_UART_1, &cfg);
 */
bsp_status_t port_uart_init(port_uart_id_t uart, const port_uart_config_t *cfg);

/**
 * @brief 反初始化指定串口，停止 DMA，释放底层资源
 * @param uart 目标串口 ID
 */
bsp_status_t port_uart_deinit(port_uart_id_t uart);

/* ================================================================
 * 发送 API
 * ================================================================ */

/**
 * @brief 阻塞写：等待发送完成后返回（超时由波特率自动推算）
 * @param uart  目标串口 ID
 * @param data  发送缓冲区
 * @param len   字节数
 */
bsp_status_t port_uart_write(port_uart_id_t uart, const uint8_t *data, uint16_t len);

/**
 * @brief 异步写：提交后立即返回，完成时触发回调
 * @param uart     目标串口 ID
 * @param data     发送缓冲区
 *                 队列模式：写入后可立即复用；直连模式：须在回调触发前保持有效
 * @param len      字节数
 * @param cb       本次传输专属完成回调（仅直连模式有效），NULL 使用 init 注册的 on_tx_complete
 * @param user_ctx 透明指针，由本次回调原样返回（仅直连模式有效）
 *
 * 示例（直连模式轮询等待）：
 *   port_uart_write_async(PORT_UART_1, buf, len, NULL, NULL);
 *   port_uart_tx_wait(PORT_UART_1, 200);
 */
bsp_status_t port_uart_write_async(port_uart_id_t uart, const uint8_t *data, uint16_t len,
                                    port_async_cb_t cb, void *user_ctx);

/**
 * @brief 查询发送通道是否忙碌
 * @param uart 目标串口 ID
 * @return true = DMA 发送尚未完成
 */
bool port_uart_is_tx_busy(port_uart_id_t uart);

/**
 * @brief 阻塞等待当前异步发送完成
 * @param uart       目标串口 ID
 * @param timeout_ms 超时时间（ms），0 表示无限等待
 * @return BSP_OK / BSP_TIMEOUT
 */
bsp_status_t port_uart_tx_wait(port_uart_id_t uart, uint32_t timeout_ms);

/* ================================================================
 * 接收控制 API
 * 数据消费直接通过上层持有的 lwrb_t * 调用 lwrb API 完成，
 * 本接口层不再暴露 receive / read / available 类包装。
 * ================================================================ */

/**
 * @brief 启动 DMA 接收（init 内部已调用，重新使能时使用）
 * @param uart 目标串口 ID
 */
bsp_status_t port_uart_enable_rx(port_uart_id_t uart);

/**
 * @brief 停止 DMA 接收
 * @param uart 目标串口 ID
 */
bsp_status_t port_uart_disable_rx(port_uart_id_t uart);

/* ================================================================
 * 错误查询 API
 * ================================================================ */

/**
 * @brief 查询最近一次硬件错误标志（调用后自动清除）
 * @param uart 目标串口 ID
 * @return port_uart_error_t 标志位组合
 */
port_uart_error_t port_uart_get_error(port_uart_id_t uart);

/**
 * @brief 尝试从错误状态恢复（重启 DMA，清除硬件错误标志）
 * @param uart 目标串口 ID
 */
bsp_status_t port_uart_recover(port_uart_id_t uart);

/* ================================================================
 * 中断入口（供 USARTx_IRQHandler 调用，不对上层暴露）
 * ================================================================ */

/**
 * @brief 统一 IRQ 入口，在 USARTx_IRQHandler 中调用
 * @param uart 目标串口 ID
 */
void port_uart_irq_handler(port_uart_id_t uart);

#endif /* PORT_UART_H */

