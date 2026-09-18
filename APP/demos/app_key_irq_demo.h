/**
 * @file app_key_irq_demo.h
 * @brief 按键 EXTI 中断计数自检演示头文件
 * @note 对标 RocketPi 03_rocketpi_key_irq，演示外部中断输入模式与 ISR 最小化处理原则。
 */

#ifndef __APP_KEY_IRQ_DEMO_H
#define __APP_KEY_IRQ_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化按键 EXTI 中断演示模块
 * @note 完成 KEY1/KEY2/KEY3 的 EXTI 硬件配置与回调注册，默认软开关关闭，
 *       通过 Shell 指令 key_irq_arm 使能计数。
 * @retval BSP_OK 初始化成功
 * @retval 其他 引脚 EXTI 配置失败
 */
bsp_status_t app_key_irq_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_KEY_IRQ_DEMO_H */
