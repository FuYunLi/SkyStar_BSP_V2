/**
 * @file bsp_bus.h
 * @brief 板级共享总线仲裁器头文件
 * @note  管理板载物理复用总线（SPI2/I2S2 共享 PB10/PC2/PC3，模拟开关由
 *        PCA9555 BIT3 软件控制，软件优先级高于拨码开关）的分时归属：
 *        acquire 完成整条链路的物理与软件切换，release 仅释放占用权。
 */

#ifndef __BSP_BUS_H
#define __BSP_BUS_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 逻辑总线与归属方定义
 * ================================================================ */

typedef enum
{
    BSP_BUS_SPI2_I2S2 = 0,  /* SPI2(Flash/IMU) 与 I2S2(ES8388) 复用总线 */
    BSP_BUS_MAX
} bsp_bus_id_t;

typedef enum
{
    BSP_BUS_OWNER_NONE = 0, /* 空闲（物理侧保持上次切换结果） */
    BSP_BUS_OWNER_SPI2,     /* 归属 SPI2：W25Q128 / ICM-42688-P */
    BSP_BUS_OWNER_I2S2,     /* 归属 I2S2：ES8388 音频 */
    BSP_BUS_OWNER_MAX
} bsp_bus_owner_t;

/* ================================================================
 * API 声明
 * ================================================================ */

/**
 * @brief 申请总线归属并完成物理切换（幂等：已持有则直接返回成功）
 * @note  切换到 I2S2 时自动挂起 IMU 采样并反初始化 SPI2；切换到 SPI2 时
 *        反初始化 I2S2 并恢复 IMU 采样。
 * @param bus   逻辑总线 ID
 * @param owner 期望归属方
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_BUSY 总线被其他归属方占用
 *         - BSP_EINVAL 参数非法
 *         - 其他 切换链路中的底层错误
 */
bsp_status_t bsp_bus_acquire(bsp_bus_id_t bus, bsp_bus_owner_t owner);

/**
 * @brief 释放总线占用权（不做物理切换，物理侧保持现状直至下次 acquire）
 * @param bus   逻辑总线 ID
 * @param owner 释放方（与当前归属不符则忽略）
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_bus_release(bsp_bus_id_t bus, bsp_bus_owner_t owner);

/**
 * @brief 查询总线当前占用归属方
 * @param bus 逻辑总线 ID
 * @return bsp_bus_owner_t 当前归属方
 */
bsp_bus_owner_t bsp_bus_current(bsp_bus_id_t bus);

#endif /* __BSP_BUS_H */
