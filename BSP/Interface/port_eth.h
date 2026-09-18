/**
 * @file port_eth.h
 * @brief 以太网接口层契约（设计稿骨架，未实现未编译）
 * @note 完整设计见 Docs/ETH_lwIP移植笔记.md。CubeMX regen 生成
 *       ethernetif.c 后，本契约约束 BSP 侧的 PHY 管理与链路服务。
 *       SMI 读写复用 HAL ETH 的 PHY 寄存器 API，故本模块实现前置
 *       条件是 hal_conf 中 ETH 模块已开启。
 */

#ifndef __PORT_ETH_H
#define __PORT_ETH_H

#include "bsp_board.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 链路参数结构
 */
typedef struct
{
    bool     link_up;     /* 链路建立 */
    bool     full_duplex; /* 双工模式 */
    uint8_t  speed_mbps;  /* 10 或 100 */
} port_eth_link_t;

/**
 * @brief 以太网 MAC + SMI 初始化（时钟/GPIO/描述符由生成层就位后调用）
 */
bsp_status_t port_eth_init(void);

/**
 * @brief PHY 硬件复位时序（PE12 拉低 ≥100µs 后释放并等待稳定）
 */
bsp_status_t port_eth_phy_reset(void);

/**
 * @brief 查询链路参数（读 LAN8720 BSR/PHYSR 并解析）
 */
bsp_status_t port_eth_get_link(port_eth_link_t *link);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_ETH_H */
