/**
 * @file bsp_uart.h
 * @brief 板级串口服务层头文件
 * @note 提供基于环形缓冲区封装的串口收发黑盒接口，屏蔽底层逻辑 ID 和硬件细节。
 */

#ifndef BSP_UART_H
#define BSP_UART_H

#include "bsp_board.h"

/**
 * @brief 初始化串口配置
 */
void bsp_uart_init(void);

/**
 * @brief 从串口接收环形缓冲区读取数据
 * @param buf 存储读取数据的缓冲区指针
 * @param len 期望读取的字节数
 * @return uint16_t 实际读取的字节数
 */
uint16_t bsp_uart_read(uint8_t *buf, uint16_t len);

/**
 * @brief 异步向串口发送数据
 * @param data 待发送数据的缓冲区指针
 * @param len 待发送数据的长度
 * @return bsp_status_t 发送状态码，BSP_OK 表示成功，BSP_EINVAL 表示无效参数
 */
bsp_status_t bsp_uart_write(const uint8_t *data, uint16_t len);

/**
 * @brief 获取当前串口发送队列的空闲空间大小
 * @return uint16_t 空闲字节数
 */
uint16_t bsp_uart_get_tx_free_space(void);

/**
 * @brief 阻塞等待串口 TX 队列彻底发完（含 DMA 完成）
 * @param timeout_ms 超时时间 (ms)，0 = 无限等待
 * @return BSP_OK / BSP_ETIMEOUT
 */
bsp_status_t bsp_uart_wait_tx_done(uint32_t timeout_ms);

#endif /* BSP_UART_H */
