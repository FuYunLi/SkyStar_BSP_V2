/**
 * @file multi_timer_port.c
 * @brief MultiTimer 平台适配层
 * @note 为 MultiTimer 提供 STM32 平台的时间戳函数
 */

#include "MultiTimer.h"
#include "port_tick.h"

/* ==================== 平台时间戳函数 ==================== */

static uint64_t multi_timer_get_ticks(void)
{
    return (uint64_t)port_tick_get_ms();
}

/* ==================== 初始化 ==================== */

void multi_timer_port_init(void)
{
    multiTimerInstall(multi_timer_get_ticks);
}
