/**
 * @file soft_onewire.h
 * @brief 软件 OneWire 单总线位时序接口头文件
 * @note 对标 RocketPi 单总线例程与官方出厂例程（ds18b20_cli）。
 *       时序基于 DWT 微秒延时，引脚方向经 MODER 寄存器直写切换。
 *       单总线要求外部约 4.7kΩ 上拉（板载 PA8 网络由模块侧提供）。
 */

#ifndef __SOFT_ONEWIRE_H
#define __SOFT_ONEWIRE_H

#include "bsp_board.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 单总线逻辑通道 */
typedef enum
{
    PORT_OW_1 = 0,  /* PA8 单总线网络 */
    PORT_OW_MAX
} port_ow_id_t;

/**
 * @brief 初始化单总线引脚（复用端口时钟已由 port_gpio_init 使能）
 */
bsp_status_t soft_onewire_init(port_ow_id_t id);

/**
 * @brief 复位脉冲并检测从机应答
 * @param[out] presence true = 检测到从机应答脉冲
 * @retval BSP_OK 时序执行完成（presence 反映检测结果）
 */
bsp_status_t soft_onewire_reset(port_ow_id_t id, bool *presence);

/**
 * @brief 写一个字节（低位在前）
 */
bsp_status_t soft_onewire_write_byte(port_ow_id_t id, uint8_t byte);

/**
 * @brief 读一个字节（低位在前）
 */
bsp_status_t soft_onewire_read_byte(port_ow_id_t id, uint8_t *byte);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_ONEWIRE_H */
