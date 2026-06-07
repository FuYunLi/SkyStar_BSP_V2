/**
 * @file dev_key.c
 * @brief 按键板级支持实现
 * @note 对接 MultiButton 中间件
 */

#include "dev_key.h"

// MultiButton 按键实例
static Button btn[DEV_KEY_MAX];

/* 定义按键硬件配置结构体 */
typedef struct
{
    port_gpio_id_t pin_id;
    uint8_t        active_level;
} dev_key_map_t;

/* 硬件配置表 */
static const dev_key_map_t key_mapping[] = {
    [DEV_KEY1] = { PORT_GPIO_KEY1, 1 },
    [DEV_KEY2] = { PORT_GPIO_KEY2, 1 },
    [DEV_KEY3] = { PORT_GPIO_KEY3, 1 }
};

/**
 * @brief 读取指定按键的电平状态
 * @param key_id 按键 ID
 * @return 按键状态 (1=按下, 0=松开)
 */
uint8_t dev_key_read(dev_key_id_t key_id)
{
    if (key_id >= DEV_KEY_MAX)
        return 0;
    port_gpio_state_t state = PORT_GPIO_LOW;
    // 读取物理电平
    if (port_gpio_read(key_mapping[key_id].pin_id, &state) != BSP_OK)
    {
        return 0;
    }

    // 直接强转枚举为 uint8_t (0 或 1) 返回给 MultiButton
    return (uint8_t)state;
}

/**
 * @brief MultiButton 读取回调 (带 button_id 参数)
 */
static uint8_t read_key_cb(uint8_t button_id)
{
    return dev_key_read((dev_key_id_t)button_id);
}

/**
 * @brief 初始化按键
 */
void dev_key_init(void)
{
    port_gpio_init();
    for (uint8_t i = 0; i < DEV_KEY_MAX; i++)
    {
        button_init(&btn[i], read_key_cb, key_mapping[i].active_level, i);
        button_start(&btn[i]);
    }
}

/**
 * @brief 按键扫描 (周期调用，建议 5ms)
 * @note 放入定时器中断或主循环
 */
void dev_key_scan(void)
{
    button_ticks();
}

/**
 * @brief 注册按键事件回调
 * @param key_id 按键 ID
 * @param event  事件类型 (ButtonEvent 枚举)
 * @param cb     回调函数
 */
void dev_key_attach(dev_key_id_t key_id, ButtonEvent event, BtnCallback cb)
{
    if (key_id >= DEV_KEY_MAX)
        return;
    button_attach(&btn[key_id], event, cb);
}
