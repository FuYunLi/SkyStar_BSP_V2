/**
 * @file port_critical.c
 * @brief 临界区保护实现（裸机版本）
 * @note 使用 PRIMASK 寄存器控制全局中断
 */

#include "port_critical.h"
#include "stm32f4xx_hal.h"

/* ==================== 静态变量 ==================== */

static uint32_t s_primask = 0;

/* ==================== 接口实现 ==================== */

void port_enter_critical(void)
{
    s_primask = __get_PRIMASK();
    __disable_irq();
}

void port_exit_critical(void)
{
    __set_PRIMASK(s_primask);
}
