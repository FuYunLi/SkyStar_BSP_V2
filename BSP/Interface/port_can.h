/**
 * @file port_can.h
 * @brief CAN 接口层头文件
 * @note 对标官方出厂 CAN 例程（can1-loopback-project）。bxCAN 外设，
 *       CAN1 挂 APB1（42MHz），PD0=RX、PD1=TX（AF9），板载 TJA1042T
 *       隔离收发器透明转发。500kbps 预分频按 12 tq/位计算。
 *       【未验证】CAN 总线尚未实测；环回模式无需对端即可自测。
 */

#ifndef __PORT_CAN_H
#define __PORT_CAN_H

#include "bsp_board.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 类型定义
 * ================================================================ */

typedef enum
{
    PORT_CAN_1 = 0, /* CAN1（PD0/PD1，隔离收发器） */
    PORT_CAN_MAX
} port_can_id_t;

/**
 * @brief CAN 帧结构（标准数据帧）
 */
typedef struct
{
    uint32_t id;          /* 标准 ID（11 位） */
    uint8_t  dlc;         /* 数据长度 0-8 */
    uint8_t  data[8];     /* 数据域 */
} port_can_frame_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化 CAN 接口（500kbps，全过滤器开放）
 * @param id CAN 逻辑 ID
 * @param loopback true = 环回模式（无需对端，发送帧直接进接收 FIFO）
 * @retval BSP_OK 初始化成功
 */
bsp_status_t port_can_init(port_can_id_t id, bool loopback);

/**
 * @brief 反初始化 CAN
 */
bsp_status_t port_can_deinit(port_can_id_t id);

/**
 * @brief 发送标准数据帧（阻塞等待空闲邮箱）
 * @param id CAN 逻辑 ID
 * @param frame 帧指针
 * @retval BSP_OK 已投入硬件发送
 * @retval BSP_EINVAL 参数无效
 * @retval BSP_ERROR 无空闲邮箱
 */
bsp_status_t port_can_send(port_can_id_t id, const port_can_frame_t *frame);

/**
 * @brief 轮询接收一帧（读接收 FIFO0）
 * @param id CAN 逻辑 ID
 * @param frame 帧回传指针
 * @retval BSP_OK 收到一帧
 * @retval BSP_ENODEV FIFO 空
 */
bsp_status_t port_can_poll_recv(port_can_id_t id, port_can_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_CAN_H */
