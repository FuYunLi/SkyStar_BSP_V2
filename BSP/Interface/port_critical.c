/**
 * @file port_critical.c
 * @brief 临界区保护实现（裸机版本）
 * @note 使用 PRIMASK 寄存器控制全局中断，支持嵌套调用
 */

#include "port_critical.h"
#include "stm32f4xx_hal.h"

/* ==================== 接口实现 ==================== */

/**
 * @brief 进入临界区
 * @return PRIMASK 值，需传给 port_exit_critical 恢复
 */
uint32_t port_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

/**
 * @brief 退出临界区
 * @param primask 进入临界区时返回的 PRIMASK 值
 */
void port_exit_critical(uint32_t primask)
{
    __set_PRIMASK(primask);
}
