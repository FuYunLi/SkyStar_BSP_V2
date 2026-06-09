/**
 * @file dev_pca9555.h
 * @brief PCA9555 16位 I2C IO 扩展器驱动头文件
 * @note 采用面向对象设计支持多芯片实例化，支持影子寄存器缓冲与引脚控制
 */

#ifndef __DEV_PCA9555_H
#define __DEV_PCA9555_H

#include "bsp_board.h"
#include "port_i2c.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 线性引脚编号定义 (0-7 对应 Port0, 8-15 对应 Port1) */
typedef enum
{
    DEV_PCA9555_PIN_0 = 0,
    DEV_PCA9555_PIN_1,
    DEV_PCA9555_PIN_2,
    DEV_PCA9555_PIN_3,
    DEV_PCA9555_PIN_4,
    DEV_PCA9555_PIN_5,
    DEV_PCA9555_PIN_6,
    DEV_PCA9555_PIN_7,
    DEV_PCA9555_PIN_8,
    DEV_PCA9555_PIN_9,
    DEV_PCA9555_PIN_10,
    DEV_PCA9555_PIN_11,
    DEV_PCA9555_PIN_12,
    DEV_PCA9555_PIN_13,
    DEV_PCA9555_PIN_14,
    DEV_PCA9555_PIN_15,
    DEV_PCA9555_PIN_MAX
} dev_pca9555_pin_t;

/* 引脚电平定义 */
typedef enum
{
    DEV_PCA9555_RESET = 0,
    DEV_PCA9555_SET   = 1
} dev_pca9555_state_t;

/* PCA9555 驱动实例结构体 */
typedef struct
{
    port_i2c_id_t i2c_id;
    uint8_t       dev_addr;
    uint8_t       shadow_output[2];
    uint8_t       shadow_config[2];
} dev_pca9555_t;

bsp_status_t dev_pca9555_init(dev_pca9555_t *dev, port_i2c_id_t i2c_id, uint8_t dev_addr);
bsp_status_t dev_pca9555_set_dir(dev_pca9555_t *dev, uint8_t port, uint8_t dir);
bsp_status_t dev_pca9555_set_pin_dir(dev_pca9555_t *dev, uint8_t port, uint8_t pin, uint8_t is_input);
bsp_status_t dev_pca9555_set_pin_dir_ex(dev_pca9555_t *dev, dev_pca9555_pin_t pin, uint8_t is_input);
bsp_status_t dev_pca9555_write_port(dev_pca9555_t *dev, uint8_t port, uint8_t value);
bsp_status_t dev_pca9555_read_port(dev_pca9555_t *dev, uint8_t port, uint8_t *value);
bsp_status_t dev_pca9555_write_pin(dev_pca9555_t *dev, uint8_t port, uint8_t pin, dev_pca9555_state_t state);
bsp_status_t dev_pca9555_read_pin(dev_pca9555_t *dev, uint8_t port, uint8_t pin, dev_pca9555_state_t *state);
bsp_status_t dev_pca9555_write_pin_ex(dev_pca9555_t *dev, dev_pca9555_pin_t pin, dev_pca9555_state_t state);
bsp_status_t dev_pca9555_read_pin_ex(dev_pca9555_t *dev, dev_pca9555_pin_t pin, dev_pca9555_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_PCA9555_H */
