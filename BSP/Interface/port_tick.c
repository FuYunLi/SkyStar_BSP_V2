/**
 * @file port_tick.c
 * @brief 系统时基接口实现
 * @note 基于 HAL_GetTick()
 */

#include "port_tick.h"
#include "stm32f4xx_hal.h"

void port_tick_init(void)
{
    /* HAL 库已初始化 SysTick，无需额外操作 */
}

uint32_t port_tick_get_ms(void)
{
    return HAL_GetTick();
}

void port_tick_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}
