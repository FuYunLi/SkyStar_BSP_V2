/**
 * @file soft_i2c.h
 * @brief 软件模拟 I2C 接口层头文件
 * @note 遵循 SkyStar BSP V2 规范与 V1 兼容性设计，仅向外暴露事务级接口
 */

#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H

#include "port_gpio.h"
#include <stdint.h>

#define SOFT_I2C_STRETCH_TIMEOUT_US (2000U)

typedef struct
{
    port_gpio_id_t scl;
    port_gpio_id_t sda;
    uint8_t        delay;  /* 半周期延时 (us)，5=100kHz，1=400kHz */
} soft_i2c_t;

void soft_i2c_init(soft_i2c_t *i2c);
bsp_status_t soft_i2c_transmit(soft_i2c_t *i2c, uint8_t addr, const uint8_t *data, uint16_t len);
bsp_status_t soft_i2c_receive(soft_i2c_t *i2c, uint8_t addr, uint8_t *data, uint16_t len);
bsp_status_t soft_i2c_mem_write(soft_i2c_t *i2c, uint8_t addr, uint16_t reg, uint8_t reg_size, const uint8_t *data, uint16_t len);
bsp_status_t soft_i2c_mem_read(soft_i2c_t *i2c, uint8_t addr, uint16_t reg, uint8_t reg_size, uint8_t *data, uint16_t len);
void soft_i2c_reset_bus(soft_i2c_t *i2c);

#endif /* __SOFT_I2C_H */
