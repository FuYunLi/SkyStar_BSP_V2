/**
 * @file bsp_led.c
 * @brief 扩展底板 PCA9555 控制的 LED 驱动实现
 * @note 实现底板 8 路 LED 逻辑控制，支持影子寄存器缓冲与多任务安全
 */

#include "bsp_led.h"
#include "dev_pca9555.h"

/* ================================================================
 * PCA9555 硬件配置宏定义
 * ================================================================ */
#define BSP_LED_PCA9555_I2C    PORT_I2C_1   /* PCA9555 挂载的 I2C 总线 ID */
#define BSP_LED_PCA9555_ADDR   0x40U        /* PCA9555 的 8 位写地址 */
#define BSP_LED_PCA9555_PORT   1U           /* LED 挂载在 PCA9555 的 Port 1 */
#define BSP_LED_PORT_DIR_OUT   0x00U        /* Port 1 全部为输出方向 */
#define BSP_LED_PORT_ALL_OFF   0xFFU        /* 输出全高电平，关闭所有 LED */

/* 全局唯一底板 PCA9555 驱动实例，供给外设控制和 Shell 调试 */
dev_pca9555_t g_pca_led;

/**
 * @brief 初始化底板 PCA9555 挂载的 8 路 LED
 * @retval BSP_OK 初始化成功
 * @retval 其他 初始化失败
 */
bsp_status_t bsp_led_init(void)
{
    // 默认连接在硬件 I2C1 通道，7-bit 地址为 0x20，对应的 8-bit 写地址为 0x40
    bsp_status_t ret = dev_pca9555_init(&g_pca_led, BSP_LED_PCA9555_I2C, BSP_LED_PCA9555_ADDR);
    if (ret != BSP_OK)
    {
        return ret;
    }

    // 设置 PCA9555 Port 1 (P1.0 - P1.7) 为输出方向 (0x00 代表全部输出)
    ret = dev_pca9555_set_dir(&g_pca_led, BSP_LED_PCA9555_PORT, BSP_LED_PORT_DIR_OUT);
    if (ret != BSP_OK)
    {
        return ret;
    }

    // 低电平点亮，默认全输出高电平 (0xFF) 以关闭底板 8 路 LED
    ret = dev_pca9555_write_port(&g_pca_led, BSP_LED_PCA9555_PORT, BSP_LED_PORT_ALL_OFF);
    return ret;
}

/**
 * @brief 设置指定 LED 的状态
 * @param led_id LED ID 索引 (BSP_LED_1 ~ BSP_LED_8)
 * @param state  目标开关状态 (BSP_LED_OFF 或 BSP_LED_ON)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t bsp_led_set(bsp_led_id_t led_id, bsp_led_state_t state)
{
    /* 入口参数安全保护 */
    if (led_id >= BSP_LED_MAX || (state != BSP_LED_OFF && state != BSP_LED_ON))
    {
        return BSP_EINVAL;
    }

    // 扩展板 LED_1 到 LED_8 (对应 Port 1 的 Pin 0 到 Pin 7)，低电平点亮
    dev_pca9555_state_t pca_state = (state == BSP_LED_ON) ? DEV_PCA9555_RESET : DEV_PCA9555_SET;
    return dev_pca9555_write_pin(&g_pca_led, BSP_LED_PCA9555_PORT, (uint8_t)led_id, pca_state);
}

/**
 * @brief 翻转指定 LED 的开关状态
 * @param led_id LED ID 索引
 * @retval BSP_OK 翻转成功
 * @retval BSP_EINVAL 参数非法
 */
bsp_status_t bsp_led_toggle(bsp_led_id_t led_id)
{
    /* 入口参数安全保护 */
    if (led_id >= BSP_LED_MAX)
    {
        return BSP_EINVAL;
    }

    // 充分利用影子寄存器进行位翻转：直接读取 shadow 对应 Bit
    uint8_t pin = (uint8_t)led_id;
    dev_pca9555_state_t cur_state = (g_pca_led.shadow_output[BSP_LED_PCA9555_PORT] & (1 << pin)) ? DEV_PCA9555_SET : DEV_PCA9555_RESET;
    dev_pca9555_state_t next_state = (cur_state == DEV_PCA9555_SET) ? DEV_PCA9555_RESET : DEV_PCA9555_SET;
    return dev_pca9555_write_pin(&g_pca_led, BSP_LED_PCA9555_PORT, pin, next_state);
}
