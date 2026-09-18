/**
 * @file app_io_check_demo.h
 * @brief 扩展 IO 批量检查自检演示头文件
 * @note 对标 RocketPi 42_rocketpi_extern_io_check。原版以 18 个物理
 *       GPIO 批量翻转做出厂检测，筑基板的对应角色由 PCA9555 IO1 口
 *       扩展的 8 颗白色状态 LED 承担（bsp_led 封装）。
 */

#ifndef __APP_IO_CHECK_DEMO_H
#define __APP_IO_CHECK_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化扩展 IO 批量检查演示模块
 * @retval BSP_OK 初始化成功
 */
bsp_status_t app_io_check_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_IO_CHECK_DEMO_H */
