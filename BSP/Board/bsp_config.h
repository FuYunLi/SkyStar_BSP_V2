/**
 * @file bsp_config.h
 * @brief 板级配置文件
 * @note 系统时钟、调试开关等配置
 */

#ifndef __BSP_CONFIG_H
#define __BSP_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 系统时钟 ==================== */

#define BSP_SYSCLK_FREQ (168000000U)
#define BSP_HCLK_FREQ   (168000000U)
#define BSP_PCLK1_FREQ  (42000000U)
#define BSP_PCLK2_FREQ  (84000000U)

/* ==================== 调试配置 ==================== */

#define BSP_DEBUG_UART  1

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CONFIG_H */
