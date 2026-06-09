/**
 * @file port_i2c.h
 * @brief I2C 物理层/软件模拟层抽象接口
 * @note 遵循 SkyStar BSP V2 规范，统一硬件与软件 I2C 访问入口
 */

#ifndef __PORT_I2C_H
#define __PORT_I2C_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    /* 硬件 I2C 通道空间 (0 ~ 0x7F) */
    PORT_I2C_1 = 0,
    PORT_I2C_HW_MAX,            /* 硬件通道上限哨兵 */

    /* 软件 I2C 通道空间 (以 0x80 起始偏置) */
    PORT_I2C_SOFT_1 = 0x80,
    PORT_I2C_SOFT_MAX,          /* 软件通道上限哨兵 */

    PORT_I2C_MAX                /* 逻辑通道大上限哨兵 */
} port_i2c_id_t;

/* 向下兼容宏定义，保留对旧版命名 PORT_I2C_SOFT 的引用 */
#define PORT_I2C_SOFT PORT_I2C_SOFT_1

bsp_status_t port_i2c_init(port_i2c_id_t id);
bsp_status_t port_i2c_mem_read(port_i2c_id_t id, uint8_t dev_addr, uint16_t mem_addr, uint8_t mem_size, uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t port_i2c_mem_write(port_i2c_id_t id, uint8_t dev_addr, uint16_t mem_addr, uint8_t mem_size, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t port_i2c_read(port_i2c_id_t id, uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t port_i2c_write(port_i2c_id_t id, uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t port_i2c_reset_bus(port_i2c_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_I2C_H */
