/**
 * @file bsp_ec11.h
 * @brief EC11 旋转编码器板级支持服务头文件
 * @note 提供面向应用层的旋转参数获取与重置接口，支持硬件选通与底层驱动状态管理。
 */

#ifndef __BSP_EC11_H
#define __BSP_EC11_H

#include <stdint.h>
#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 结构体定义
 * ================================================================ */

/**
 * @brief 板级编码器数据结构体
 */
typedef struct
{
    int32_t count;      /* 当前的编码器计步总值 */
    int8_t dir;         /* 最后一次旋转的方向：1为顺时针(CW)，-1为逆时针(CCW)，0为无旋转 */
} bsp_ec11_info_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化 EC11 编码器板级服务，配置模拟开关物理通道并启动驱动
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
bsp_status_t bsp_ec11_init(void);

/**
 * @brief 获取编码器的最新旋转参数（获取后方向参数会自动复位）
 * @param info 存储旋转参数的结构体指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数无效
 *         - BSP_ERROR 服务未初始化
 */
bsp_status_t bsp_ec11_get_info(bsp_ec11_info_t *info);

/**
 * @brief 重置板级编码器服务及底层的计数值与方向参数
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_ERROR 服务未初始化
 */
bsp_status_t bsp_ec11_reset_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_EC11_H */
