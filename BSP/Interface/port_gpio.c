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
    [PORT_GPIO_EC11_B] = {GPIOD, GPIO_PIN_13},
    [PORT_GPIO_HCSR04_TRIG] = {GPIOD, GPIO_PIN_11},
    [PORT_GPIO_HCSR04_ECHO] = {GPIOA, GPIO_PIN_8},
    [PORT_GPIO_HX711_DOUT] = {GPIOB, GPIO_PIN_0},
    [PORT_GPIO_HX711_SCK] = {GPIOB, GPIO_PIN_1},
    [PORT_GPIO_RS485_DE] = {GPIOD, GPIO_PIN_15},
    [PORT_GPIO_STEPPER_DIR] = {GPIOD, GPIO_PIN_4},
    [PORT_GPIO_STEPPER_ENN] = {GPIOD, GPIO_PIN_7}
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

    /* 位操作类外设的引脚模式初始化（CubeMX 未配置的引脚在此集中托管）：
     * HX711 时钟为推挽输出且默认低（数据手册要求空闲低电平），
     * DOUT 为浮空输入，RS485 方向脚默认接收态 */
    {
        GPIO_InitTypeDef gpio_init = {0};

        gpio_init.Pin = gpio_mapping[PORT_GPIO_HX711_SCK].pin;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        gpio_init.Pull = GPIO_NOPULL;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        (void)HAL_GPIO_Init(gpio_mapping[PORT_GPIO_HX711_SCK].port, &gpio_init);

        gpio_init.Pin = gpio_mapping[PORT_GPIO_HX711_DOUT].pin;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_NOPULL;
        (void)HAL_GPIO_Init(gpio_mapping[PORT_GPIO_HX711_DOUT].port, &gpio_init);

        gpio_init.Pin = gpio_mapping[PORT_GPIO_RS485_DE].pin;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        (void)HAL_GPIO_Init(gpio_mapping[PORT_GPIO_RS485_DE].port, &gpio_init);

        gpio_init.Pin = gpio_mapping[PORT_GPIO_STEPPER_DIR].pin |
                        gpio_mapping[PORT_GPIO_STEPPER_ENN].pin;
        gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
        (void)HAL_GPIO_Init(gpio_mapping[PORT_GPIO_STEPPER_DIR].port, &gpio_init);

        port_gpio_write(PORT_GPIO_HX711_SCK, PORT_GPIO_LOW);
        port_gpio_write(PORT_GPIO_RS485_DE, PORT_GPIO_LOW);
        /* 步进默认方向正转、使能脚拉高（ENN 低有效 = 初始禁用） */
        port_gpio_write(PORT_GPIO_STEPPER_DIR, PORT_GPIO_LOW);
        port_gpio_write(PORT_GPIO_STEPPER_ENN, PORT_GPIO_HIGH);
    }

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
 * @brief 由物理引脚号推导所属 EXTI 中断线的中断号
 * @param pin_num 物理引脚号 (0-15)
 * @return IRQn_Type 对应的中断号
 */
static IRQn_Type s_exti_get_irqn(uint32_t pin_num)
{
    if (pin_num <= 4U)
    {
        return (IRQn_Type)(EXTI0_IRQn + (IRQn_Type)pin_num);
    }

    if (pin_num <= 9U)
    {
        return EXTI9_5_IRQn;
    }

    return EXTI15_10_IRQn;
}

/**
 * @brief 初始化外部中断：注册业务回调并完成全套硬件配置
 * @note 硬件原理：SYSCFG_EXTICR 将 EXTI 线路由至对应 GPIO 端口，边沿触发器检测输入
 *       后经中断屏蔽寄存器 (EXTI_IMR) 提交给 NVIC。本函数独立完成路由、边沿、
 *       屏蔽与 NVIC 使能，不依赖 CubeMX 预先使能 EXTI。
 *       注意：若同一 EXTI 线后续在 CubeMX 中使能了其他引脚，重新生成代码后
 *       须移除本文件末尾对应的中断入口定义，避免重复链接。
 * @param pin_id 逻辑引脚 ID
 * @param trigger 触发方式（上升沿/下降沿/双边沿）
 * @param cb 业务中断回调函数指针，ISR 上下文执行，须保持极短
 * @retval BSP_OK 初始化成功
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

    if (cb == NULL)
    {
        return BSP_EINVAL;
    }

    GPIO_TypeDef *port = gpio_mapping[pin_id].port;
    uint16_t pin_mask = gpio_mapping[pin_id].pin;

    /* 由 GPIO_PIN_x 位掩码反查物理引脚号 */
    uint32_t pin_num = 0U;
    while (((uint32_t)pin_mask >> pin_num) != 1UL)
    {
        pin_num++;
    }

    /* 校验触发方式合法后再动硬件，避免无效参数产生半配置状态 */
    if ((trigger != PORT_EXTI_TRIGGER_RISING) && (trigger != PORT_EXTI_TRIGGER_FALLING) &&
        (trigger != PORT_EXTI_TRIGGER_BOTH))
    {
        return BSP_EINVAL;
    }

    /* 使能 SYSCFG 时钟，EXTI 引脚源选择寄存器挂载其下 */
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    /* 将 EXTI 线路由至引脚所在 GPIO 端口（A=0 B=1 C=2 D=3 E=4，端口间距 0x400） */
    uint32_t port_idx = ((uint32_t)port - GPIOA_BASE) / 0x0400UL;
    uint32_t cr_idx = pin_num >> 2U;
    uint32_t cr_pos = (pin_num & 0x3UL) * 4UL;
    SYSCFG->EXTICR[cr_idx] = (SYSCFG->EXTICR[cr_idx] & ~(0xFUL << cr_pos)) | (port_idx << cr_pos);

    /* 清除历史挂起标志，防止注册瞬间残留电平误触发 */
    EXTI->PR = BIT(pin_num);

    /* 按触发方式配置上升/下降沿选择寄存器 */
    switch (trigger)
    {
    case PORT_EXTI_TRIGGER_RISING:
        EXTI->RTSR |= BIT(pin_num);
        EXTI->FTSR &= ~BIT(pin_num);
        break;

    case PORT_EXTI_TRIGGER_FALLING:
        EXTI->RTSR &= ~BIT(pin_num);
        EXTI->FTSR |= BIT(pin_num);
        break;

    case PORT_EXTI_TRIGGER_BOTH:
        EXTI->RTSR |= BIT(pin_num);
        EXTI->FTSR |= BIT(pin_num);
        break;

    default:
        return BSP_EINVAL;
    }

    /* 解除中断屏蔽（仅中断不挂事件），并使能 NVIC */
    EXTI->IMR |= BIT(pin_num);
    exti_callbacks[pin_id] = cb;

    HAL_NVIC_SetPriority(s_exti_get_irqn(pin_num), 5U, 0U);
    HAL_NVIC_EnableIRQ(s_exti_get_irqn(pin_num));

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

/* ================================================================
 * 中断入口
 * CubeMX 仅生成了 EXTI2/EXTI3（触摸与 RTC 中断）的入口，
 * 以下为本模块自行使能线路（按键 KEY1/KEY2/KEY3）补齐的入口。
 * 若后续在 CubeMX 中使能同线引脚的 EXTI，须移除对应定义避免重复链接。
 * ================================================================ */

/**
 * @brief EXTI 线 0 中断入口（KEY1/PA0）
 */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

/**
 * @brief EXTI 线 5-9 中断入口（KEY2/PE8 使用线 8）
 * @note 遍历挂起寄存器分发给本组内已使能的引脚，兼容同组多引脚扩展
 */
void EXTI9_5_IRQHandler(void)
{
    for (uint32_t pin = 5U; pin <= 9U; pin++)
    {
        if ((EXTI->PR & BIT(pin)) != 0U)
        {
            HAL_GPIO_EXTI_IRQHandler((uint16_t)BIT(pin));
        }
    }
}

/**
 * @brief EXTI 线 10-15 中断入口（KEY3/PC13 使用线 13）
 * @note 遍历挂起寄存器分发给本组内已使能的引脚，兼容同组多引脚扩展
 */
void EXTI15_10_IRQHandler(void)
{
    for (uint32_t pin = 10U; pin <= 15U; pin++)
    {
        if ((EXTI->PR & BIT(pin)) != 0U)
        {
            HAL_GPIO_EXTI_IRQHandler((uint16_t)BIT(pin));
        }
    }
}
