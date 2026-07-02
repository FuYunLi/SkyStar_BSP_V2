/**
 * @file port_gpio.c
 * @brief GPIO 接口层实现
 * @note 封装 HAL 库的 GPIO 操作，提供逻辑 ID 映射及中断分发。
 */

#include "port_gpio.h"
#include "gpio.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} port_gpio_map_t;

/* 物理引脚逻辑映射表，统一缩进且不进行无意义的空格对齐 */
static const port_gpio_map_t gpio_mapping[] =
{
    [PORT_GPIO_LED_CORE] = {GPIOB, GPIO_PIN_8},
    [PORT_GPIO_KEY1] = {GPIOA, GPIO_PIN_0},
    [PORT_GPIO_KEY2] = {GPIOE, GPIO_PIN_8},
    [PORT_GPIO_KEY3] = {GPIOC, GPIO_PIN_13},
    [PORT_GPIO_BUZZER] = {GPIOA, GPIO_PIN_6},
    [PORT_GPIO_LCD_CS] = {GPIOE, GPIO_PIN_14},
    [PORT_GPIO_LCD_DC] = {GPIOD, GPIO_PIN_14},
    [PORT_GPIO_LCD_RST] = {GPIOE, GPIO_PIN_1},
    [PORT_GPIO_TOUCH_SCL] = {GPIOD, GPIO_PIN_10},
    [PORT_GPIO_TOUCH_SDA] = {GPIOE, GPIO_PIN_13},
    [PORT_GPIO_TOUCH_INT] = {GPIOE, GPIO_PIN_2},
    [PORT_GPIO_W25Q_CS] = {GPIOE, GPIO_PIN_4},
    [PORT_GPIO_SD3078_INT] = {GPIOE, GPIO_PIN_3},
    [PORT_GPIO_IMU_CS] = {GPIOE, GPIO_PIN_7},
    [PORT_GPIO_EC11_A] = {GPIOD, GPIO_PIN_12},
    [PORT_GPIO_EC11_B] = {GPIOD, GPIO_PIN_13}
};

/* 外部中断业务回调函数表 */
static port_exti_callback_t exti_callbacks[PORT_GPIO_MAX] = {NULL};

/**
 * @brief 初始化 GPIO 接口层，安全开启所有映射端口的时钟
 * @retval BSP_OK 初始化成功
 */
bsp_status_t port_gpio_init(void)
{
    /* 安全使能引脚映射中使用的全部 GPIO 端口时钟，防范 BusFault */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();

    /* 初始化输出引脚的安全默认电平 */
    port_gpio_write(PORT_GPIO_LED_CORE, PORT_GPIO_LOW);

    return BSP_OK;
}

/**
 * @brief 读取指定逻辑引脚的电平状态
 * @note 硬件原理：通过读取输入数据寄存器 (GPIOx_IDR) 对应的位获取物理引脚的输入电平。
 * @param pin_id 逻辑引脚 ID
 * @param state 用于存储读取电平的指针
 * @retval BSP_OK 读取成功
 * @retval BSP_EINVAL 无效的参数或引脚未映射
 */
bsp_status_t port_gpio_read(port_gpio_id_t pin_id, port_gpio_state_t *state)
{
    if (pin_id >= PORT_GPIO_MAX)
    {
        return BSP_EINVAL;
    }

    if (gpio_mapping[pin_id].port == NULL)
    {
        return BSP_EINVAL;
    }

    if (state == NULL)
    {
        return BSP_EINVAL;
    }

    GPIO_PinState val = HAL_GPIO_ReadPin(gpio_mapping[pin_id].port, gpio_mapping[pin_id].pin);
    *state = (val == GPIO_PIN_SET) ? PORT_GPIO_HIGH : PORT_GPIO_LOW;

    return BSP_OK;
}

/**
 * @brief 写入指定逻辑引脚的电平状态
 * @note 硬件原理：通过向端口位设置/清除寄存器 (GPIOx_BSRR) 写入特定数值控制 ODR，从而改变物理引脚输出电平。
 * @param pin_id 逻辑引脚 ID
 * @param state 要写入的电平状态
 * @retval BSP_OK 写入成功
 * @retval BSP_EINVAL 无效的引脚 ID 或引脚未映射
 */
bsp_status_t port_gpio_write(port_gpio_id_t pin_id, port_gpio_state_t state)
{
    if (pin_id >= PORT_GPIO_MAX)
    {
        return BSP_EINVAL;
    }

    if (gpio_mapping[pin_id].port == NULL)
    {
        return BSP_EINVAL;
    }

    HAL_GPIO_WritePin(gpio_mapping[pin_id].port, gpio_mapping[pin_id].pin, (GPIO_PinState)state);

    return BSP_OK;
}

/**
 * @brief 翻转指定逻辑引脚的电平状态
 * @note 硬件原理：翻转输出数据寄存器 (GPIOx_ODR) 对应的位以改变物理引脚输出电平。
 * @param pin_id 逻辑引脚 ID
 * @retval BSP_OK 翻转成功
 * @retval BSP_EINVAL 无效的引脚 ID 或引脚未映射
 */
bsp_status_t port_gpio_toggle(port_gpio_id_t pin_id)
{
    if (pin_id >= PORT_GPIO_MAX)
    {
        return BSP_EINVAL;
    }

    if (gpio_mapping[pin_id].port == NULL)
    {
        return BSP_EINVAL;
    }

    HAL_GPIO_TogglePin(gpio_mapping[pin_id].port, gpio_mapping[pin_id].pin);

    return BSP_OK;
}

/**
 * @brief 注册外部中断的回调函数
 * @note 硬件原理：外部中断由 SYSCFG 进行引脚源选择，输入边缘触发器检测输入并提交给 NVIC。本函数仅绑定业务回调。
 * @param pin_id 逻辑引脚 ID
 * @param trigger 触发方式（上升沿/下降沿/双边沿）
 * @param cb 业务中断回调函数指针
 * @retval BSP_OK 注册成功
 * @retval BSP_EINVAL 参数无效或引脚未映射
 *
 * 示例：
 *   void on_key_pressed(void)
 *   {
 *       port_gpio_toggle(PORT_GPIO_LED_CORE);
 *   }
 *   port_gpio_exti_init(PORT_GPIO_KEY1, PORT_EXTI_TRIGGER_FALLING, on_key_pressed);
 */
bsp_status_t port_gpio_exti_init(port_gpio_id_t pin_id, port_exti_trigger_t trigger, port_exti_callback_t cb)
{
    if (pin_id >= PORT_GPIO_MAX)
    {
        return BSP_EINVAL;
    }

    if (gpio_mapping[pin_id].port == NULL)
    {
        return BSP_EINVAL;
    }

    exti_callbacks[pin_id] = cb;

    return BSP_OK;
}

/**
 * @brief 重写 HAL 库的 EXTI 回调入口，将外部中断路由分发至注册好的业务回调
 * @param GPIO_Pin 发生中断的物理引脚编号
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    for (int i = 0; i < PORT_GPIO_MAX; i++)
    {
        if (gpio_mapping[i].pin == GPIO_Pin)
        {
            if (exti_callbacks[i] != NULL)
            {
                exti_callbacks[i]();
            }
        }
    }
}
