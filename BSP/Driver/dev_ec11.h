/**
 * @file dev_ec11.h
 * @brief EC11 旋转编码器底层设备驱动头文件
 * @note 封装外部中断正交状态机解码接口，提供纯净的计数值与旋转方向数据读取服务。
 */

#ifndef __DEV_EC11_H
#define __DEV_EC11_H

#include <stdint.h>
#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 结构体定义
 * ================================================================ */

/**
 * @brief EC11 编码器信息结构体
 */
typedef struct
{
    int32_t count;      /* 累积旋转计数值 */
    int8_t dir;         /* 最后一次旋转的方向：1为顺时针(CW)，-1为逆时针(CCW)，0为无旋转 */
} dev_ec11_info_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化 EC11 旋转编码器底层物理引脚与状态机参数
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 */
bsp_status_t dev_ec11_init(void);

/**
 * @brief 获取当前 EC11 编码器的累积计数值与最后一次旋转方向
 * @param info 存储编码器信息的结构体指针
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - BSP_EINVAL 参数指针无效
 */
bsp_status_t dev_ec11_get_info(dev_ec11_info_t *info);

/**
 * @brief 重置 EC11 旋转编码器的计数值及方向参数
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 */
bsp_status_t dev_ec11_reset_count(void);

/**
 * @brief EXTI 外部中断底部分发处理入口函数，在外部中断服务程序中被显式调用
 * @param GPIO_Pin 触发中断的引脚编号
 */
void dev_ec11_irq_handler(uint16_t GPIO_Pin);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_EC11_H */
