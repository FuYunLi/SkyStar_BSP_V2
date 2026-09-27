/**
 * @file boot_hw.h
 * @brief Bootloader 基础硬件服务头文件（时钟 + 调试串口 + 毫秒时基）
 * @note 纯寄存器实现，不依赖 HAL。USART1(PA9/PA10) 为调试口。
 */

#ifndef __BOOT_HW_H
#define __BOOT_HW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 时钟与外设初始化（HSE→PLL 168MHz，APB1=42MHz，APB2=84MHz） */
void boot_hw_clock_init(void);

/* 调试串口初始化（USART1 115200-8-N-1） */
void boot_hw_uart_init(void);

/* 轮询打印以 NUL 结尾的字符串 */
void boot_hw_print(const char *str);

/* 轮询打印十六进制数值（8 位） */
void boot_hw_print_hex(uint32_t value);

/* 毫秒级时基（SysTick 计数，供 W25Q 忙等超时使用） */
void     boot_hw_delay_ms(uint32_t ms);
uint32_t boot_hw_tick_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_HW_H */
