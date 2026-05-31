/**
 * @file port_tick.h
 * @brief 系统时基接口
 * @note 提供毫秒级时间获取功能
 */

#ifndef __PORT_TICK_H
#define __PORT_TICK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化系统时基
 */
void port_tick_init(void);

/**
 * @brief 获取系统运行时间（毫秒）
 * @return 系统运行毫秒数
 */
uint32_t port_tick_get_ms(void);

/**
 * @brief 毫秒级阻塞延时
 * @param ms 延时毫秒数
 */
void port_tick_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_TICK_H */
