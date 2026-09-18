/**
 * @file soft_onewire.c
 * @brief 软件 OneWire 单总线位时序接口实现
 * @note 时序参数依据 Maxim DS18B20 数据手册标准档：
 *       复位 480µs 拉低 + 70µs 采样应答 + 410µs 恢复；
 *       写时隙 ≥60µs（写 1 先拉低 6µs 再释放，写 0 拉低 60µs）；
 *       读时隙拉低 6µs 后 9µs 处采样，时隙总长 ≥60µs。
 *       引脚方向经 MODER 寄存器直写切换（输出推挽 / 输入浮空），
 *       释放总线依赖外部上拉。
 */

#include "soft_onewire.h"
#include "port_dwt.h"
#include "stm32f4xx_hal.h"

/* ================================================================
 * 私有变量
 * ================================================================ */

/* 单总线引脚映射（与 port_gpio 中 HX711 等位操作外设同风格） */
static GPIO_TypeDef *const s_ow_port[PORT_OW_MAX] = { [PORT_OW_1] = GPIOA };
static const uint16_t s_ow_pin[PORT_OW_MAX] = { [PORT_OW_1] = GPIO_PIN_8 };

/* 时序常量（微秒） */
#define OW_RESET_LOW_US   (480U)
#define OW_PRESENCE_US    (70U)
#define OW_RESET_TAIL_US  (410U)
#define OW_SLOT_LOW_US    (6U)
#define OW_WRITE0_LOW_US  (60U)
#define OW_READ_SAMPLE_US (9U)
#define OW_SLOT_TAIL_US   (55U)

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 引脚切换为推挽输出
 */
static void s_ow_set_output(port_ow_id_t id)
{
    GPIO_TypeDef *port = s_ow_port[id];
    uint32_t pin = 0U;
    uint16_t mask = s_ow_pin[id];

    while (((uint32_t)mask >> pin) != 1UL)
    {
        pin++;
    }

    port->MODER = (port->MODER & ~(3UL << (2U * pin))) | (1UL << (2U * pin));
}

/**
 * @brief 引脚切换为浮空输入（释放总线，依赖外部上拉）
 */
static void s_ow_set_input(port_ow_id_t id)
{
    GPIO_TypeDef *port = s_ow_port[id];
    uint32_t pin = 0U;
    uint16_t mask = s_ow_pin[id];

    while (((uint32_t)mask >> pin) != 1UL)
    {
        pin++;
    }

    port->MODER &= ~(3UL << (2U * pin));
}

/**
 * @brief 输出指定电平（须处于输出模式）
 */
static void s_ow_write_level(port_ow_id_t id, bool high)
{
    if (high)
    {
        s_ow_port[id]->BSRR = s_ow_pin[id];
    }
    else
    {
        s_ow_port[id]->BSRR = (uint32_t)s_ow_pin[id] << 16U;
    }
}

/**
 * @brief 读取引脚电平
 */
static bool s_ow_read_level(port_ow_id_t id)
{
    return ((s_ow_port[id]->IDR & s_ow_pin[id]) != 0U);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化单总线引脚
 */
bsp_status_t soft_onewire_init(port_ow_id_t id)
{
    if (id >= PORT_OW_MAX)
    {
        return BSP_EINVAL;
    }

    /* 复位后默认输出高（随后立即进入复位时序） */
    s_ow_set_output(id);
    s_ow_write_level(id, true);

    return BSP_OK;
}

/**
 * @brief 复位脉冲并检测从机应答
 */
bsp_status_t soft_onewire_reset(port_ow_id_t id, bool *presence)
{
    if ((id >= PORT_OW_MAX) || (presence == NULL))
    {
        return BSP_EINVAL;
    }

    s_ow_set_output(id);
    s_ow_write_level(id, false);
    port_dwt_delay_us(OW_RESET_LOW_US);

    /* 释放总线，70µs 后采样从机应答脉冲（应答时从机拉低） */
    s_ow_set_input(id);
    port_dwt_delay_us(OW_PRESENCE_US);
    *presence = !s_ow_read_level(id);
    port_dwt_delay_us(OW_RESET_TAIL_US);

    return BSP_OK;
}

/**
 * @brief 写一个字节（低位在前）
 */
bsp_status_t soft_onewire_write_byte(port_ow_id_t id, uint8_t byte)
{
    if (id >= PORT_OW_MAX)
    {
        return BSP_EINVAL;
    }

    s_ow_set_output(id);

    for (uint32_t bit = 0U; bit < 8U; bit++)
    {
        if ((byte & (uint8_t)(1U << bit)) != 0U)
        {
            /* 写 1：短拉低后尽早释放 */
            s_ow_write_level(id, false);
            port_dwt_delay_us(OW_SLOT_LOW_US);
            s_ow_set_input(id);
        }
        else
        {
            /* 写 0：拉低贯穿整个时隙 */
            s_ow_write_level(id, false);
            port_dwt_delay_us(OW_WRITE0_LOW_US);
            s_ow_set_input(id);
        }

        port_dwt_delay_us(OW_SLOT_TAIL_US);
    }

    return BSP_OK;
}

/**
 * @brief 读一个字节（低位在前）
 */
bsp_status_t soft_onewire_read_byte(port_ow_id_t id, uint8_t *byte)
{
    if ((id >= PORT_OW_MAX) || (byte == NULL))
    {
        return BSP_EINVAL;
    }

    *byte = 0U;

    for (uint32_t bit = 0U; bit < 8U; bit++)
    {
        s_ow_set_output(id);
        s_ow_write_level(id, false);
        port_dwt_delay_us(OW_SLOT_LOW_US);

        /* 释放后 9µs 处采样：从机在此时隙内驱动数据位 */
        s_ow_set_input(id);
        port_dwt_delay_us(OW_READ_SAMPLE_US);

        if (s_ow_read_level(id))
        {
            *byte |= (uint8_t)(1U << bit);
        }

        port_dwt_delay_us(OW_SLOT_TAIL_US);
    }

    return BSP_OK;
}
