/**
 * @file port_encoder.h
 * @brief 定时器正交编码器接口层头文件
 * @note 提供统一的定时器正交编码器操作抽象，隔离底层定时器硬件及 HAL 库细节。
 */

#ifndef __PORT_ENCODER_H
#define __PORT_ENCODER_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 类型定义
 * ================================================================ */

/**
 * @brief 编码器逻辑通道 ID 枚举
 */
typedef enum
{
    PORT_ENCODER_EC11 = 0,  /* EC11 编码器通道 (TIM4) */
    PORT_ENCODER_MAX
} port_encoder_id_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化硬件编码器接口层
 * @param[in] id 编码器逻辑 ID
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 句柄未初始化或硬件状态异常
 */
bsp_status_t port_encoder_init(port_encoder_id_t id);

/**
 * @brief 反初始化硬件编码器接口层
 * @param[in] id 编码器逻辑 ID
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 */
bsp_status_t port_encoder_deinit(port_encoder_id_t id);

/**
 * @brief 开启指定编码器通道的硬件计数
 * @param[in] id 编码器逻辑 ID
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 启动失败
 */
bsp_status_t port_encoder_start(port_encoder_id_t id);

/**
 * @brief 停止指定编码器通道的硬件计数
 * @param[in] id 编码器逻辑 ID
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 停止失败
 */
bsp_status_t port_encoder_stop(port_encoder_id_t id);

/**
 * @brief 设置指定编码器通道的当前计数值
 * @param[in] id 编码器逻辑 ID
 * @param[in] val 目标计数值
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 */
bsp_status_t port_encoder_set_count(port_encoder_id_t id, uint16_t val);

/**
 * @brief 获取指定编码器通道的当前计数值
 * @param[in] id 编码器逻辑 ID
 * @param[out] val 存储计数值的指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 获取失败
 */
bsp_status_t port_encoder_get_raw_count(port_encoder_id_t id, uint16_t *val);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_ENCODER_H */
