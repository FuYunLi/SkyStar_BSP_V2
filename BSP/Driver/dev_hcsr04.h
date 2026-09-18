/**
 * @file dev_hcsr04.h
 * @brief HC-SR04 超声波测距驱动头文件
 * @note 对标 RocketPi 19_rocketpi_hcsr04。测距原理：TRIG 发出 ≥10µs
 *       触发脉冲后，模块发出 8 个 40kHz 声波并在 ECHO 上输出高电平，
 *       高电平持续时长即声波往返时间，距离 = 时间 × 343m/s ÷ 2。
 *       【未验证】外部模块尚未接线实测，示例引脚 PD11(TRIG)/PA8(ECHO)。
 */

#ifndef __DEV_HCSR04_H
#define __DEV_HCSR04_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 执行一次测距（阻塞式，最大约 60ms）
 * @param[out] distance_mm 距离回传指针，单位毫米；超时无回波时回填 0
 * @retval BSP_OK 测距成功
 * @retval BSP_EINVAL 参数无效
 * @retval BSP_ETIMEOUT 回波超时（目标过远或未接线）
 */
bsp_status_t dev_hcsr04_read_mm(uint32_t *distance_mm);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_HCSR04_H */
