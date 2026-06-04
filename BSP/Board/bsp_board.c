/**
 * @file bsp_board.c
 * @brief 板级支持包核心实现
 */

#include "bsp_board.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "port_uart.h"

/* 调试串口ID宏定义 */
#define PORT_UART_DEBUG PORT_UART_1

/* ================================================================
 * 状态码字符串
 * ================================================================ */

const char *bsp_status_str(bsp_status_t status)
{
    switch (status)
    {
    case BSP_OK:
        return "OK";
    case BSP_ERROR:
        return "ERROR";
    case BSP_EINVAL:
        return "EINVAL";
    case BSP_ENOMEM:
        return "ENOMEM";
    case BSP_ETIMEOUT:
        return "ETIMEOUT";
    case BSP_BUSY:
        return "BUSY";
    case BSP_EIO:
        return "EIO";
    case BSP_ENODEV:
        return "ENODEV";
    default:
        return "UNKNOWN";
    }
}

/* ================================================================
 * printf 输出重定向
 * ================================================================ */
#if defined(__ARMCC_VERSION) // Keil
/**
 * @brief 在 Arm Compiler/Keil 下，将 fputc 重定向到调试 USART。
 * @param ch 待发送字符。
 * @param f  被忽略的文件句柄。
 * @return 实际发送的字符。
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    uint8_t data = (uint8_t)ch;
    bsp_uart_write(&data, 1U);
    return ch;
}

#endif /* __ARMCC_VERSION */
