/**
 * @file bsp_usb.h
 * @brief 板级 USB CDC 路由层契约（设计稿骨架，未实现未编译）
 * @note 本头文件是 USB CDC 接入 BSP 的**契约预定义**，完整设计见
 *       Docs/USB_CDC移植笔记.md。ST 协议栈接管 PCD 后，数据边界收敛
 *       在 usbd_cdc_if.c：入口 CDC_Receive_FS → 本层 on_rx，出口
 *       CDC_Transmit_FS 由本层 send 封装。等 CubeMX regen 后按此契约
 *       实现 .c 并加入 uvprojx。
 */

#ifndef __BSP_USB_H
#define __BSP_USB_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB CDC 数据接收回调类型（由 bsp_usb 内部从 usbd_cdc_if 转入）
 * @param data 接收缓冲区（中断上下文，须尽快拷走或入队）
 * @param len 字节数
 */
typedef void (*bsp_usb_rx_cb_t)(const uint8_t *data, uint32_t len);

/**
 * @brief 注册上层接收路由（Shell/Logger 数据通道二选一）
 */
bsp_status_t bsp_usb_attach(bsp_usb_rx_cb_t cb);

/**
 * @brief 向主机发送数据（封装 CDC_Transmit_FS）
 * @note 忙时返回 BSP_BUSY，上层按需重试；不要在 ISR 中调用
 */
bsp_status_t bsp_usb_send(const uint8_t *data, uint16_t len);

/**
 * @brief 查询 USB 是否已枚举配置完成
 */
bool bsp_usb_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_USB_H */
