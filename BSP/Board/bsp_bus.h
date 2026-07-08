/**
 * @file bsp_bus.h
 * @brief 板级总线仲裁服务头文件
 * @note 用于分时共享物理冲突的引脚总线资源（如 SPI2 与 I2S2）
 */

#ifndef __BSP_BUS_H
#define __BSP_BUS_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 总线通道逻辑 ID */
typedef enum
{
    BSP_BUS_SPI2_I2S2 = 0, /* SPI2 / I2S2 冲突总线 */
    BSP_BUS_MAX
} bsp_bus_id_t;

/* 总线工作模式 */
typedef enum
{
    BSP_BUS_MODE_SPI = 0,  /* SPI2 模式：分配给 Flash/IMU */
    BSP_BUS_MODE_I2S       /* I2S2 模式：分配给 ES8388 音频 */
} bsp_bus_mode_t;

/**
 * @brief  初始化总线仲裁器
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_bus_init(void);

/**
 * @brief  申请占有指定总线通道并切换到指定模式
 * @param  id 总线通道 ID
 * @param  mode 目标模式
 * @retval bsp_status_t 成功或已经占有返回 BSP_OK，被他人占有且无法释放返回 BSP_BUSY
 */
bsp_status_t bsp_bus_acquire(bsp_bus_id_t id, bsp_bus_mode_t mode);

/**
 * @brief  释放指定总线通道的占有（通常恢复到默认 SPI 模式）
 * @param  id 总线通道 ID
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t bsp_bus_release(bsp_bus_id_t id);

/**
 * @brief  获取指定总线通道的当前工作模式
 * @param  id 总线通道 ID
 * @retval bsp_bus_mode_t 当前模式
 */
bsp_bus_mode_t bsp_bus_get_mode(bsp_bus_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BUS_H */
