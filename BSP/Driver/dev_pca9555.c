/**
 * @file dev_pca9555.c
 * @brief PCA9555 16位 I2C IO 扩展器驱动实现
 * @note 采用面向对象设计与影子寄存器管理，提供多实例支持
 */

#include "dev_pca9555.h"
#include "port_critical.h"
#include <stdio.h>
#include <stdlib.h>

/* 芯片寄存器偏置地址定义 */
#define REG_IN0       0x00
#define REG_IN1       0x01
#define REG_OUT0      0x02
#define REG_OUT1      0x03
#define REG_POL0      0x04
#define REG_POL1      0x05
#define REG_CONFIG0   0x06
#define REG_CONFIG1   0x07

/* I2C 读写超时时间 (ms) */
#define PCA9555_I2C_TIMEOUT_MS   15U

/* 声明外部全局 LED 级 PCA9555 实例 */
extern dev_pca9555_t g_pca_led;

/**
 * @brief 初始化 PCA9555 实例并读回物理寄存器状态
 * @param dev PCA9555 实例指针
 * @param i2c_id I2C 逻辑端口 ID
 * @param dev_addr 从机 8-bit 地址
 * @retval BSP_OK 初始化成功
 * @retval BSP_EINVAL 参数非法
 * @retval 其他 硬件通信异常
 * @note 依据 PCA9555 手册 6.2 节，初始化将自动读回 REG_OUT 和 REG_CONFIG 状态以支持系统热复位
 * @example
 * dev_pca9555_t my_pca;
 * dev_pca9555_init(&my_pca, PORT_I2C_1, 0x40);
 */
bsp_status_t dev_pca9555_init(dev_pca9555_t *dev, port_i2c_id_t i2c_id, uint8_t dev_addr)
{
    /* 注入入口安全防线 */
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    dev->i2c_id = i2c_id;
    dev->dev_addr = dev_addr;

    // 确保底层总线初始化完成
    bsp_status_t ret = port_i2c_init(i2c_id);
    if (ret != BSP_OK)
    {
        return ret;
    }

    // 读回输出寄存器 Port 0 & Port 1 的上电默认状态 (0xFF)
    ret = port_i2c_mem_read(i2c_id, dev_addr, REG_OUT0, 1, dev->shadow_output, 2, PCA9555_I2C_TIMEOUT_MS);
    if (ret != BSP_OK)
    {
        return ret;
    }

    // 读回方向配置寄存器 Port 0 & Port 1 的上电默认状态 (0xFF)
    ret = port_i2c_mem_read(i2c_id, dev_addr, REG_CONFIG0, 1, dev->shadow_config, 2, PCA9555_I2C_TIMEOUT_MS);
    return ret;
}

/**
 * @brief 设置 PCA9555 逻辑 Port 的方向配置
 * @param dev PCA9555 实例指针
 * @param port 端口号 (0 或 1)
 * @param dir 方向掩码 (1=输入, 0=输出)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_set_dir(dev_pca9555_t *dev, uint8_t port, uint8_t dir)
{
    /* 注入入口安全防线 */
    if (dev == NULL || port > 1)
    {
        return BSP_EINVAL;
    }

    dev->shadow_config[port] = dir;
    uint8_t reg = (port == 0) ? REG_CONFIG0 : REG_CONFIG1;
    // 根据手册 6.3 节，配置方向寄存器控制每个 IO 的输入(1)/输出(0)方向
    return port_i2c_mem_write(dev->i2c_id, dev->dev_addr, reg, 1, &dev->shadow_config[port], 1, PCA9555_I2C_TIMEOUT_MS);
}

/**
 * @brief 设置单个引脚的方向配置
 * @param dev PCA9555 实例指针
 * @param port 端口号 (0 或 1)
 * @param pin 引脚号 (0 - 7)
 * @param is_input 是否为输入 (1=输入, 0=输出)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_set_pin_dir(dev_pca9555_t *dev, uint8_t port, uint8_t pin, uint8_t is_input)
{
    /* 注入入口安全防线 */
    if (dev == NULL || port > 1 || pin > 7)
    {
        return BSP_EINVAL;
    }

    uint32_t primask = port_enter_critical();
    if (is_input != 0)
    {
        dev->shadow_config[port] |= (1 << pin);
    }
    else
    {
        dev->shadow_config[port] &= ~(1 << pin);
    }
    port_exit_critical(primask);

    uint8_t reg = (port == 0) ? REG_CONFIG0 : REG_CONFIG1;
    // 根据手册 6.3 节，配置方向寄存器控制每个 IO 的输入(1)/输出(0)方向
    return port_i2c_mem_write(dev->i2c_id, dev->dev_addr, reg, 1, &dev->shadow_config[port], 1, PCA9555_I2C_TIMEOUT_MS);
}

/**
 * @brief 线性编号设置单个引脚的方向配置 (面向对象线性接口)
 * @param dev PCA9555 实例指针
 * @param pin 线性引脚编号 (DEV_PCA9555_PIN_0 到 DEV_PCA9555_PIN_15)
 * @param is_input 是否为输入 (1=输入, 0=输出)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_set_pin_dir_ex(dev_pca9555_t *dev, dev_pca9555_pin_t pin, uint8_t is_input)
{
    /* 注入入口安全防线 */
    if (dev == NULL || pin >= DEV_PCA9555_PIN_MAX)
    {
        return BSP_EINVAL;
    }

    return dev_pca9555_set_pin_dir(dev, (uint8_t)(pin / 8), (uint8_t)(pin % 8), is_input);
}

/**
 * @brief 写入指定逻辑 Port 的 8-bit 输出电平值
 * @param dev PCA9555 实例指针
 * @param port 端口号 (0 或 1)
 * @param value 8位输出电平
 * @retval BSP_OK 写入成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_write_port(dev_pca9555_t *dev, uint8_t port, uint8_t value)
{
    /* 注入入口安全防线 */
    if (dev == NULL || port > 1)
    {
        return BSP_EINVAL;
    }

    dev->shadow_output[port] = value;
    uint8_t reg = (port == 0) ? REG_OUT0 : REG_OUT1;
    // 根据手册 6.2 节，向输出寄存器写数据可以拉高或拉低对应引脚
    return port_i2c_mem_write(dev->i2c_id, dev->dev_addr, reg, 1, &dev->shadow_output[port], 1, PCA9555_I2C_TIMEOUT_MS);
}

/**
 * @brief 读取指定逻辑 Port 的 8-bit 输入电平值
 * @param dev PCA9555 实例指针
 * @param port 端口号 (0 或 1)
 * @param value 接收缓冲指针
 * @retval BSP_OK 读取成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_read_port(dev_pca9555_t *dev, uint8_t port, uint8_t *value)
{
    /* 注入入口安全防线 */
    if (dev == NULL || port > 1 || value == NULL)
    {
        return BSP_EINVAL;
    }

    uint8_t reg = (port == 0) ? REG_IN0 : REG_IN1;
    // 根据手册 6.1 节，读取输入寄存器可以采集对应引脚的物理逻辑电平
    return port_i2c_mem_read(dev->i2c_id, dev->dev_addr, reg, 1, value, 1, PCA9555_I2C_TIMEOUT_MS);
}

/**
 * @brief 控制单个引脚的输出电平 (面向对象接口)
 * @param dev PCA9555 实例指针
 * @param port 端口号 (0 或 1)
 * @param pin 引脚号 (0 - 7)
 * @param state 电平状态 (DEV_PCA9555_RESET 或 DEV_PCA9555_SET)
 * @retval BSP_OK 写入成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_write_pin(dev_pca9555_t *dev, uint8_t port, uint8_t pin, dev_pca9555_state_t state)
{
    /* 注入入口安全防线 */
    if (dev == NULL || port > 1 || pin > 7)
    {
        return BSP_EINVAL;
    }

    uint32_t primask = port_enter_critical();
    if (state == DEV_PCA9555_SET)
    {
        dev->shadow_output[port] |= (1 << pin);
    }
    else
    {
        dev->shadow_output[port] &= ~(1 << pin);
    }
    port_exit_critical(primask);

    uint8_t reg = (port == 0) ? REG_OUT0 : REG_OUT1;
    // 影子寄存器机制：单向修改本地影子映射，一并刷写入芯片
    return port_i2c_mem_write(dev->i2c_id, dev->dev_addr, reg, 1, &dev->shadow_output[port], 1, PCA9555_I2C_TIMEOUT_MS);
}

/**
 * @brief 读取单个引脚的物理输入电平 (面向对象接口)
 * @param dev PCA9555 实例指针
 * @param port 端口号 (0 或 1)
 * @param pin 引脚号 (0 - 7)
 * @param state 接收电平缓冲指针
 * @retval BSP_OK 读取成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_read_pin(dev_pca9555_t *dev, uint8_t port, uint8_t pin, dev_pca9555_state_t *state)
{
    /* 注入入口安全防线 */
    if (dev == NULL || port > 1 || pin > 7 || state == NULL)
    {
        return BSP_EINVAL;
    }

    uint8_t reg_val = 0;
    bsp_status_t ret = dev_pca9555_read_port(dev, port, &reg_val);
    if (ret != BSP_OK)
    {
        return ret;
    }

    *state = ((reg_val >> pin) & 0x01) ? DEV_PCA9555_SET : DEV_PCA9555_RESET;
    return BSP_OK;
}

/**
 * @brief 线性编号写单个引脚 (面向对象线性接口)
 * @param dev PCA9555 实例指针
 * @param pin 线性引脚编号 (DEV_PCA9555_PIN_0 到 DEV_PCA9555_PIN_15)
 * @param state 电平状态
 * @retval BSP_OK 写入成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_write_pin_ex(dev_pca9555_t *dev, dev_pca9555_pin_t pin, dev_pca9555_state_t state)
{
    /* 注入入口安全防线 */
    if (dev == NULL || pin >= DEV_PCA9555_PIN_MAX)
    {
        return BSP_EINVAL;
    }

    return dev_pca9555_write_pin(dev, (uint8_t)(pin / 8), (uint8_t)(pin % 8), state);
}

/**
 * @brief 线性编号读单个引脚 (面向对象线性接口)
 * @param dev PCA9555 实例指针
 * @param pin 线性引脚编号
 * @param state 接收电平缓冲指针
 * @retval BSP_OK 读取成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t dev_pca9555_read_pin_ex(dev_pca9555_t *dev, dev_pca9555_pin_t pin, dev_pca9555_state_t *state)
{
    /* 注入入口安全防线 */
    if (dev == NULL || pin >= DEV_PCA9555_PIN_MAX || state == NULL)
    {
        return BSP_EINVAL;
    }

    return dev_pca9555_read_pin(dev, (uint8_t)(pin / 8), (uint8_t)(pin % 8), state);
}


