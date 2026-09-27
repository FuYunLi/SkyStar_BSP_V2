/**
 * @file system_boot.c
 * @brief Bootloader 系统初始化（极简替代 HAL 的 system_stm32f4xx.c）
 * @note 仅启用 FPU 访问位并报告时钟；真正的时钟配置在 boot_clock.c
 *       的 boot_clock_init() 中以寄存器方式完成。
 */

#include <stdint.h>
#include "stm32f407xx.h"

uint32_t SystemCoreClock = 168000000U;

/**
 * @brief 启动文件调用的系统初始化（此时时钟仍为 HSI 16MHz）
 */
void SystemInit(void)
{
    /* 使能 CP10/CP11 全访问（FPU），供编译器潜在使用 */
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2));
    __DSB();
    __ISB();
}
