/**
 * @file bsp_uart.c
 * @brief 板级串口服务层源文件
 * @note 内部统一分配 LwRB 和 DMA 缓冲区，屏蔽底层物理串口细节，对外提供标准流式读写接口。
 */

#include "bsp_uart.h"
#include "port_uart.h"
#include <stdio.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */
#define UART_BAUD_RATE  115200U
#define RX_DMA_BUF_SIZE 64U
#define RX_RB_BUF_SIZE  1024U
#define TX_RB_BUF_SIZE  2048U

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

/* ================================================================
 * 私有函数声明
 * ================================================================ */

static void uart_rx_data_cb(port_uart_id_t uart, uint16_t len, void *user_ctx);
static void uart_error_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx);

/* ================================================================
 * 私有函数实现
 * ================================================================ */

/**
 * @brief 接收数据回调（串口空闲中断触发）
 */
static void uart_rx_data_cb(port_uart_id_t uart, uint16_t len, void *user_ctx)
{
    (void)uart;
    (void)len;
    (void)user_ctx;
}

/**
 * @brief 错误回调
 */
static void uart_error_cb(uint8_t bus_id, bsp_status_t result, void *user_ctx)
{
    (void)bus_id;
    (void)user_ctx;
    // 发生硬件错误时使用基础 printf 输出以保持可靠性
    printf("[UART ERROR] Bus: %u, Result: %d\r\n", (unsigned int)bus_id, (int)result);
}

/**
 * @brief 初始化板级串口服务层
 * @note 内部自动初始化 LwRB 并开启 DMA 接收与发送
 */
void bsp_uart_init(void)
{
    /* 初始化接收与发送缓冲区 */
    lwrb_init(&s_rx_rb, s_rx_rb_buf, sizeof(s_rx_rb_buf));
    lwrb_init(&s_tx_rb, s_tx_rb_buf, sizeof(s_tx_rb_buf));

    /* 填充串口配置 */
    port_uart_config_t cfg = { .baudrate        = UART_BAUD_RATE,
                               .rx_dma_buf      = s_rx_dma_buf,
                               .rx_dma_buf_size = sizeof(s_rx_dma_buf),
                               .rx_rb           = &s_rx_rb,
                               .tx_rb           = &s_tx_rb,
                               .on_tx_complete  = NULL,
                               .on_error        = uart_error_cb,
                               .on_rx_data      = uart_rx_data_cb,
                               .user_ctx        = NULL };

    /* 注册并开启串口隔离层 */
    port_uart_init(PORT_UART_1, &cfg);
}

/**
 * @brief 从串口接收环形缓冲区读取数据
 * @param buf 存储读取数据的缓冲区指针
 * @param len 期望读取的字节数
 * @return uint16_t 实际读取的字节数
 */
uint16_t bsp_uart_read(uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0U)
    {
        return 0U;
    }
    return lwrb_read(&s_rx_rb, buf, len);
}

/**
 * @brief 异步向串口发送数据
 * @param data 待发送数据的缓冲区指针
 * @param len 待发送数据的长度
 * @return bsp_status_t 发送状态码，BSP_OK 表示成功，BSP_EINVAL 表示无效参数
 */
bsp_status_t bsp_uart_write(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U)
    {
        return BSP_EINVAL;
    }

    // 依托底层接口内部的 port_enter_critical，实现多任务并发写 LwRB 的绝对安全
    return port_uart_write_async(PORT_UART_1, data, len, NULL, NULL);
}

/**
 * @brief 获取当前串口发送队列的空闲空间大小
 */
uint16_t bsp_uart_get_tx_free_space(void)
{
    return (uint16_t)lwrb_get_free(&s_tx_rb);
}
