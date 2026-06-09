/**
 * @file soft_i2c.c
 * @brief 软件 I2C 接口实现
 * @note 基于 GPIO 位操作 + DWT 微秒延时
 */

#include "soft_i2c.h"
#include "port_dwt.h"

/* GPIO 操作宏定义 */
#define SDA_HIGH(i2c) port_gpio_write((i2c)->sda, PORT_GPIO_HIGH)
#define SDA_LOW(i2c)  port_gpio_write((i2c)->sda, PORT_GPIO_LOW)
#define SCL_HIGH(i2c) port_gpio_write((i2c)->scl, PORT_GPIO_HIGH)
#define SCL_LOW(i2c)  port_gpio_write((i2c)->scl, PORT_GPIO_LOW)

/* ================================================================
 * 私有辅助函数声明与实现
 * ================================================================ */

static inline void s_delay(uint32_t us)
{
    port_dwt_delay_us(us);
}

static int s_sda_read(soft_i2c_t *i2c)
{
    port_gpio_state_t state;
    /* 硬件原理：通过读取输入数据寄存器 (GPIOx_IDR) 获取 SDA 物理电平 */
    port_gpio_read(i2c->sda, &state);
    return (state == PORT_GPIO_HIGH) ? 1 : 0;
}

static int s_scl_wait_high(soft_i2c_t *i2c)
{
    port_gpio_state_t state;
    uint32_t waited = 0;

    for (;;)
    {
        /* 硬件原理：时钟拉伸机制检测，通过读取输入数据寄存器 (GPIOx_IDR) 监测 SCL 电平 */
        port_gpio_read(i2c->scl, &state);
        if (state == PORT_GPIO_HIGH)
        {
            return 0;
        }
        s_delay(1);
        if (++waited >= SOFT_I2C_STRETCH_TIMEOUT_US)
        {
            return -1;
        }
    }
}

static int s_write_bit(soft_i2c_t *i2c, int bit)
{
    if (bit)
    {
        /* 硬件原理：操作端口设置/清除寄存器 (GPIOx_BSRR) 向 SDA 写高电平 */
        SDA_HIGH(i2c);
    }
    else
    {
        /* 硬件原理：操作端口设置/清除寄存器 (GPIOx_BSRR) 向 SDA 写低电平 */
        SDA_LOW(i2c);
    }
    s_delay(i2c->delay);

    SCL_HIGH(i2c);
    if (s_scl_wait_high(i2c) != 0)
    {
        return -1;
    }
    s_delay(i2c->delay);

    SCL_LOW(i2c);
    return 0;
}

static int s_read_bit(soft_i2c_t *i2c)
{
    /* 硬件原理：释放 SDA 数据线为开漏输入状态 */
    SDA_HIGH(i2c);
    s_delay(i2c->delay);

    SCL_HIGH(i2c);
    if (s_scl_wait_high(i2c) != 0)
    {
        return -1;
    }
    s_delay(i2c->delay);

    int bit = s_sda_read(i2c);

    SCL_LOW(i2c);
    return bit;
}

static int s_start(soft_i2c_t *i2c)
{
    SDA_HIGH(i2c);
    SCL_HIGH(i2c);
    if (s_scl_wait_high(i2c) != 0)
    {
        return -1;
    }
    s_delay(i2c->delay);

    SDA_LOW(i2c);
    s_delay(i2c->delay);

    SCL_LOW(i2c);
    return 0;
}

static void s_stop(soft_i2c_t *i2c)
{
    SDA_LOW(i2c);
    s_delay(i2c->delay);

    SCL_HIGH(i2c);
    s_scl_wait_high(i2c);
    s_delay(i2c->delay);

    SDA_HIGH(i2c);
    s_delay(i2c->delay);
}

static int s_write_byte(soft_i2c_t *i2c, uint8_t data)
{
    for (int i = 7; i >= 0; i--)
    {
        if (s_write_bit(i2c, (data >> i) & 0x01) != 0)
        {
            return -1;
        }
    }
    int ack = s_read_bit(i2c);
    if (ack < 0)
    {
        return -1;
    }
    return (ack == 0) ? 0 : -1;
}

static int s_read_byte(soft_i2c_t *i2c, uint8_t *data, uint8_t ack)
{
    uint8_t val = 0;
    for (int i = 7; i >= 0; i--)
    {
        int bit = s_read_bit(i2c);
        if (bit < 0)
        {
            return -1;
        }
        val |= (uint8_t)(bit << i);
    }
    if (s_write_bit(i2c, ack ? 0 : 1) != 0)
    {
        return -1;
    }

    *data = val;
    return 0;
}

/* ================================================================
 * 公开事务级接口 API
 * ================================================================ */

/**
 * @brief 初始化软件 I2C 控制引脚默认高电平状态
 * @param i2c 软件 I2C 句柄指针
 */
void soft_i2c_init(soft_i2c_t *i2c)
{
    SDA_HIGH(i2c);
    SCL_HIGH(i2c);
}

/**
 * @brief 软件 I2C 事务级数据发送
 * @param i2c 软件 I2C 句柄指针
 * @param addr 从机 8-bit 地址
 * @param data 待发送数据缓存指针
 * @param len 待发送数据字节长度
 * @retval BSP_OK 成功
 * @retval BSP_ERROR 失败
 */
bsp_status_t soft_i2c_transmit(soft_i2c_t *i2c, uint8_t addr, const uint8_t *data, uint16_t len)
{
    if (s_start(i2c) != 0)
    {
        goto err;
    }
    if (s_write_byte(i2c, addr) != 0)
    {
        goto err;
    }
    for (uint16_t i = 0; i < len; i++)
    {
        if (s_write_byte(i2c, data[i]) != 0)
        {
            goto err;
        }
    }
    s_stop(i2c);
    return BSP_OK;
err:
    s_stop(i2c);
    return BSP_ERROR;
}

/**
 * @brief 软件 I2C 事务级数据接收
 * @param i2c 软件 I2C 句柄指针
 * @param addr 从机 8-bit 地址
 * @param data 接收缓冲区指针
 * @param len 待接收数据字节长度
 * @retval BSP_OK 成功
 * @retval BSP_ERROR 失败
 */
bsp_status_t soft_i2c_receive(soft_i2c_t *i2c, uint8_t addr, uint8_t *data, uint16_t len)
{
    if (s_start(i2c) != 0)
    {
        goto err;
    }
    if (s_write_byte(i2c, addr | 0x01) != 0)
    {
        goto err;
    }
    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t ack = (i + 1 < len) ? 1 : 0;
        if (s_read_byte(i2c, &data[i], ack) != 0)
        {
            goto err;
        }
    }
    s_stop(i2c);
    return BSP_OK;
err:
    s_stop(i2c);
    return BSP_ERROR;
}

/**
 * @brief 软件 I2C 事务级寄存器写入
 * @param i2c 软件 I2C 句柄指针
 * @param addr 从机 8-bit 地址
 * @param reg 寄存器内部地址
 * @param reg_size 寄存器物理宽度（1或2）
 * @param data 待发送数据缓存指针
 * @param len 待发送数据字节长度
 * @retval BSP_OK 成功
 * @retval BSP_ERROR 失败
 */
bsp_status_t soft_i2c_mem_write(soft_i2c_t *i2c, uint8_t addr, uint16_t reg, uint8_t reg_size, const uint8_t *data, uint16_t len)
{
    if (s_start(i2c) != 0)
    {
        goto err;
    }
    if (s_write_byte(i2c, addr) != 0)
    {
        goto err;
    }
    if (reg_size == 2)
    {
        if (s_write_byte(i2c, (uint8_t)(reg >> 8)) != 0)
        {
            goto err;
        }
    }
    if (s_write_byte(i2c, (uint8_t)(reg & 0xFF)) != 0)
    {
        goto err;
    }
    for (uint16_t i = 0; i < len; i++)
    {
        if (s_write_byte(i2c, data[i]) != 0)
        {
            goto err;
        }
    }
    s_stop(i2c);
    return BSP_OK;
err:
    s_stop(i2c);
    return BSP_ERROR;
}

/**
 * @brief 软件 I2C 事务级寄存器读取
 * @param i2c 软件 I2C 句柄指针
 * @param addr 从机 8-bit 地址
 * @param reg 寄存器内部地址
 * @param reg_size 寄存器物理宽度（1或2）
 * @param data 接收缓冲区指针
 * @param len 待接收数据字节长度
 * @retval BSP_OK 成功
 * @retval BSP_ERROR 失败
 */
bsp_status_t soft_i2c_mem_read(soft_i2c_t *i2c, uint8_t addr, uint16_t reg, uint8_t reg_size, uint8_t *data, uint16_t len)
{
    if (s_start(i2c) != 0)
    {
        goto err;
    }
    if (s_write_byte(i2c, addr) != 0)
    {
        goto err;
    }
    if (reg_size == 2)
    {
        if (s_write_byte(i2c, (uint8_t)(reg >> 8)) != 0)
        {
            goto err;
        }
    }
    if (s_write_byte(i2c, (uint8_t)(reg & 0xFF)) != 0)
    {
        goto err;
    }
    if (s_start(i2c) != 0)
    {
        goto err;
    }
    if (s_write_byte(i2c, addr | 0x01) != 0)
    {
        goto err;
    }
    for (uint16_t i = 0; i < len; i++)
    {
        uint8_t ack = (i + 1 < len) ? 1 : 0;
        if (s_read_byte(i2c, &data[i], ack) != 0)
        {
            goto err;
        }
    }
    s_stop(i2c);
    return BSP_OK;
err:
    s_stop(i2c);
    return BSP_ERROR;
}

/**
 * @brief 软件 I2C 强制清除总线挂死锁死
 * @param i2c 软件 I2C 句柄指针
 */
void soft_i2c_reset_bus(soft_i2c_t *i2c)
{
    SDA_HIGH(i2c);

    /* 硬件原理：SDA 被从机拉低锁死时，通过 SCL 连续发送脉冲使其状态机转动并释放 SDA */
    for (int i = 0; i < 9; i++)
    {
        SCL_LOW(i2c);
        s_delay(5);
        SCL_HIGH(i2c);
        s_delay(5);

        if (s_sda_read(i2c) == 1)
        {
            break;
        }
    }

    SDA_LOW(i2c);
    s_delay(5);
    SCL_HIGH(i2c);
    s_delay(5);
    SDA_HIGH(i2c);
    s_delay(5);
}
