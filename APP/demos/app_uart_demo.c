#include "app_uart_demo.h"
#include <stdio.h>
#include <string.h>
#include "port_uart.h"
#include "MultiTimer.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define RX_DMA_BUF_SIZE 64U
#define RX_RB_BUF_SIZE  128U
#define TX_RB_BUF_SIZE  256U

/* ================================================================
 * 私有变量
 * ================================================================ */

/* 接收 DMA 缓冲区 */
static uint8_t s_rx_dma_buf[RX_DMA_BUF_SIZE];

/* 接收与发送 lwrb 内存区 */
static uint8_t s_rx_rb_buf[RX_RB_BUF_SIZE];
static uint8_t s_tx_rb_buf[TX_RB_BUF_SIZE];

/* 接收与发送 lwrb 句柄 */
static lwrb_t s_rx_rb;
static lwrb_t s_tx_rb;

/* 周期发送软件定时器 */
static MultiTimer s_timer_periodic_tx;

/* ================================================================
 * 私有函数声明
 * ================================================================ */

static void uart_rx_data_cb(port_uart_id_t uart, uint16_t len, void *user_ctx);
static void uart_tx_cplt_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx);
static void uart_error_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx);
static void periodic_tx_callback(MultiTimer *timer, void *userData);

/* ================================================================
 * 私有函数实现
 * ================================================================ */

/**
 * @brief 接收数据回调（串口空闲中断触发）
 */
static void uart_rx_data_cb(port_uart_id_t uart, uint16_t len, void *user_ctx)
{
    (void)user_ctx;
    uint8_t temp_buf[64];
    uint16_t read_len;

    /* 循环读取 lwrb 缓冲区的数据，并使用异步发送发回，完成 Echo 环回测试 */
    while (1)
    {
        read_len = lwrb_read(&s_rx_rb, temp_buf, sizeof(temp_buf));
        if (read_len == 0U)
        {
            break;
        }

        /* 异步发回所收到的数据 */
        port_uart_write_async(uart, temp_buf, read_len, NULL, NULL);
    }
}

/**
 * @brief 发送完成回调
 */
static void uart_tx_cplt_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx)
{
    (void)bus_id;
    (void)result;
    (void)user_ctx;
    /* 发送完成，可在高频调试下添加计数器或用于性能分析 */
}

/**
 * @brief 错误回调
 */
static void uart_error_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx)
{
    (void)bus_id;
    (void)user_ctx;
    /* 发生硬件错误时使用基础 printf 输出以保持可靠性 */
    printf("[UART ERROR] Bus: %u, Result: %d\r\n", (unsigned int)bus_id, (int)result);
}

/**
 * @brief 周期性发送定时器回调
 */
static void periodic_tx_callback(MultiTimer *timer, void *userData)
{
    (void)userData;
    static uint32_t s_count = 0U;
    
    s_count++;

    /* 周期性使用阻塞 printf 进行基础吞吐压力测试 */
    printf("[UART DEMO] Periodic print. Count: %u, Tick: %u ms\r\n", 
           (unsigned int)s_count, (unsigned int)HAL_GetTick());

    /* 周期性通过异步队列发送大块数据 */
    const char *msg = "===> Async queue transmission block test <===\r\n";
    port_uart_write_async(PORT_UART_1, (const uint8_t *)msg, (uint16_t)strlen(msg), NULL, NULL);

    /* 重新启动定时器，1000ms 后再次触发 */
    multiTimerStart(timer, 1000U, periodic_tx_callback, NULL);
}

/* ================================================================
 * 公开函数实现
 * ================================================================ */

/**
 * @brief 初始化并运行串口测试 Demo
 */
void app_uart_demo_init(void)
{
    /* 初始化接收与发送缓冲区 */
    lwrb_init(&s_rx_rb, s_rx_rb_buf, sizeof(s_rx_rb_buf));
    lwrb_init(&s_tx_rb, s_tx_rb_buf, sizeof(s_tx_rb_buf));

    /* 填充串口配置 */
    port_uart_config_t cfg = 
    {
        .baudrate = 115200U,
        .rx_dma_buf = s_rx_dma_buf,
        .rx_dma_buf_size = sizeof(s_rx_dma_buf),
        .rx_rb = &s_rx_rb,
        .tx_rb = &s_tx_rb,
        .on_tx_complete = uart_tx_cplt_cb,
        .on_error = uart_error_cb,
        .on_rx_data = uart_rx_data_cb,
        .user_ctx = NULL
    };

    /* 注册并开启串口隔离层 */
    port_uart_init(PORT_UART_1, &cfg);

    /* 启动周期发送软件定时器（1000ms 周期） */
    multiTimerStart(&s_timer_periodic_tx, 1000U, periodic_tx_callback, NULL);
}
