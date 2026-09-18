/**
 * @file bsp_rs485.h
 * @brief 隔离 RS485 总线服务层头文件
 * @note 对标官方出厂例程 drv_rs485。半双工方向控制：发送前置 DE
 *       为高，HAL_UART_Transmit 返回前已等待 TC 标志（移位器排空），
 *       随后即可安全切回接收态。
 *       【未验证】RS485 对端尚未接线实测。
 */

#ifndef __BSP_RS485_H
#define __BSP_RS485_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化 RS485 服务（USART3 115200-8-N-1，默认接收态）
 * @retval BSP_OK 初始化成功
 */
bsp_status_t bsp_rs485_init(void);

/**
 * @brief 发送一帧数据（阻塞式，自动控制收发方向）
 * @param data 数据缓冲区
 * @param len 字节数
 * @retval BSP_OK 发送完成（含移位器排空）
 */
bsp_status_t bsp_rs485_send(const uint8_t *data, uint16_t len);

/**
 * @brief 接收一帧数据（阻塞轮询）
 * @param buf 存储接收数据的缓冲区
 * @param len 期望接收字节数
 * @param timeout_ms 超时时间（ms）
 * @retval BSP_OK 接收完成
 * @retval BSP_ETIMEOUT 超时
 */
bsp_status_t bsp_rs485_recv(uint8_t *buf, uint16_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_RS485_H */
