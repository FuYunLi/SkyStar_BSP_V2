/**
 * @file app_adc_demo.h
 * @brief 内置 ADC 与电位器 Shell 自检演示模块头文件
 * @note 导出 adc_read、pot_read 与 pot_monitor 控制台调试命令，用于硬件交互测试。
 */

#ifndef __APP_ADC_DEMO_H
#define __APP_ADC_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化内置 ADC 与电位器 Shell 自检演示模块
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 */
bsp_status_t app_adc_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ADC_DEMO_H */
