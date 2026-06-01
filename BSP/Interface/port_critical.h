/**
 * @file port_critical.h
 * @brief 临界区保护接口
 * @note 裸机环境使用中断开关实现，便于后续切换 RTOS
 */

#ifndef __PORT_CRITICAL_H
#define __PORT_CRITICAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 临界区接口 ==================== */

/**
 * @brief 进入临界区
 * @return 中断状态掩码，需传给 port_exit_critical 恢复
 * @note 裸机：关闭全局中断；RTOS：获取互斥锁或自旋锁
 */
uint32_t port_enter_critical(void);

/**
 * @brief 退出临界区
 * @param primask 进入临界区时返回的状态掩码
 * @note 裸机：恢复中断状态；RTOS：释放锁
 */
void port_exit_critical(uint32_t primask);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_CRITICAL_H */
