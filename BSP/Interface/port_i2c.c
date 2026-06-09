/**
 * @file port_i2c.c
 * @brief I2C 接口层实现
 * @note 统一分发层：根据总线 ID 路由到硬件 I2C（HAL）或软件 I2C，完美隔离物理引脚
 */

#include "port_i2c.h"
#include "soft_i2c.h"
#include "i2c.h"
#include "port_dwt.h"

/* ================================================================
 * 物理引脚时钟控制包装函数
 * ================================================================ */

static void s_i2c1_clk_disable(void)
{
    __HAL_RCC_I2C1_CLK_DISABLE();
}

/* ================================================================
 * 硬件/软件外设与引脚映射表定义
 * ================================================================ */

typedef struct
{
    GPIO_TypeDef *scl_port;
    uint16_t     scl_pin;
    GPIO_TypeDef *sda_port;
    uint16_t     sda_pin;
    void         (*clk_disable)(void);
} port_i2c_hw_pin_t;

static I2C_HandleTypeDef *hw_mapping[] = {
    [PORT_I2C_1] = &hi2c1,
};

static const port_i2c_hw_pin_t hw_pins[] = {
    [PORT_I2C_1] = {
        .scl_port    = GPIOB,
        .scl_pin     = GPIO_PIN_6,
        .sda_port    = GPIOB,
        .sda_pin     = GPIO_PIN_7,
        .clk_disable = s_i2c1_clk_disable,
    }
};

static soft_i2c_t sw_mapping[] = {
    [PORT_I2C_SOFT_1 - PORT_I2C_SOFT_1] = {
        .scl   = PORT_GPIO_TOUCH_SCL,
        .sda   = PORT_GPIO_TOUCH_SDA,
        .delay = 2,
    }
};

/* ================================================================
 * 私有路由与重置辅助函数
 * ================================================================ */

static I2C_HandleTypeDef *get_hw(port_i2c_id_t bus)
{
    /* 确保逻辑 ID 落在硬件 I2C 通道区间内 */
    if (bus >= PORT_I2C_HW_MAX)
    {
        return NULL;
    }

    /* 采用数组实际大小进行防溢出安全保护 */
    if (bus >= (port_i2c_id_t)(sizeof(hw_mapping) / sizeof(hw_mapping[0])))
    {
        return NULL;
    }
    return hw_mapping[bus];
}

static soft_i2c_t *get_sw(port_i2c_id_t bus)
{
    /* 确保逻辑 ID 落在软件 I2C 通道区间内 */
    if (bus < PORT_I2C_SOFT_1 || bus >= PORT_I2C_SOFT_MAX)
    {
        return NULL;
    }

    uint32_t idx = (uint32_t)(bus - PORT_I2C_SOFT_1);
    /* 采用软件通道数组的实际大小进行防溢出保护 */
    if (idx >= sizeof(sw_mapping) / sizeof(sw_mapping[0]))
    {
        return NULL;
    }
    return &sw_mapping[idx];
}

static void s_hw_i2c_reset_gpio(port_i2c_id_t bus)
{
    /* 确保逻辑 ID 落在硬件 I2C 通道区间内 */
    if (bus >= PORT_I2C_HW_MAX)
    {
        return;
    }

    /* 采用数组实际大小进行防溢出安全保护 */
    if (bus >= (port_i2c_id_t)(sizeof(hw_pins) / sizeof(hw_pins[0])))
    {
        return;
    }
    const port_i2c_hw_pin_t *cfg = &hw_pins[bus];
    if (cfg->scl_port == NULL)
    {
        return;
    }

    GPIO_InitTypeDef gpio_init_struct = {0};

    /* 硬件原理：暂时关闭硬件 I2C 模块外设时钟，防止物理逻辑冲突 */
    if (cfg->clk_disable != NULL)
    {
        cfg->clk_disable();
    }

    /* 硬件原理：通过 GPIO_Init 接管硬件引脚为普通 GPIO 输出开漏模式，利用 GPIOx_BSRR 寄存器控制输出 */
    gpio_init_struct.Pin   = cfg->scl_pin;
    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_init_struct.Pull  = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(cfg->scl_port, &gpio_init_struct);

    gpio_init_struct.Pin   = cfg->sda_pin;
    HAL_GPIO_Init(cfg->sda_port, &gpio_init_struct);

    /* 硬件原理：置高 SDA 以释放总线状态 */
    HAL_GPIO_WritePin(cfg->sda_port, cfg->sda_pin, GPIO_PIN_SET);

    /* 硬件原理：SCL 产生 9 个脉冲以便从机能够释放被锁死的 SDA 数据线 */
    for (int i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(cfg->scl_port, cfg->scl_pin, GPIO_PIN_RESET);
        port_dwt_delay_us(5U);
        HAL_GPIO_WritePin(cfg->scl_port, cfg->scl_pin, GPIO_PIN_SET);
        port_dwt_delay_us(5U);

        /* 硬件原理：通过读取输入数据寄存器 (GPIOx_IDR) 监测 SDA 物理电平是否释放为高电平 */
        if (HAL_GPIO_ReadPin(cfg->sda_port, cfg->sda_pin) == GPIO_PIN_SET)
        {
            break;
        }
    }

    /* 硬件原理：发出一个标准的 STOP 物理波形以使得从机 I2C 状态机恢复初始闲置状态 */
    HAL_GPIO_WritePin(cfg->sda_port, cfg->sda_pin, GPIO_PIN_RESET);
    port_dwt_delay_us(5U);
    HAL_GPIO_WritePin(cfg->scl_port, cfg->scl_pin, GPIO_PIN_SET);
    port_dwt_delay_us(5U);
    HAL_GPIO_WritePin(cfg->sda_port, cfg->sda_pin, GPIO_PIN_SET);
    port_dwt_delay_us(5U);

    /* 重新恢复初始化对应的硬件 I2C 外设以准备正常的通信 */
    if (bus == PORT_I2C_1)
    {
        HAL_I2C_DeInit(get_hw(bus));
        MX_I2C1_Init();
    }
}

/* ================================================================
 * 公开接口 API
 * ================================================================ */

/**
 * @brief  初始化 I2C 端口
 * @param  id 指定的 I2C 逻辑端口 ID
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_i2c_init(port_i2c_id_t id)
{
    soft_i2c_t *sw = get_sw(id);
    if (sw != NULL)
    {
        soft_i2c_init(sw);
        return BSP_OK;
    }

    I2C_HandleTypeDef *hw = get_hw(id);
    if (hw != NULL)
    {
        if (HAL_I2C_GetState(hw) == HAL_I2C_STATE_RESET)
        {
            return BSP_ERROR;
        }
        return BSP_OK;
    }

    return BSP_EINVAL;
}

/**
 * @brief  I2C 寄存器块读取 (标准融合接口)
 * @param  id I2C 逻辑端口 ID
 * @param  dev_addr 从机 8-bit 地址
 * @param  mem_addr 内部寄存器目标地址
 * @param  mem_size 寄存器地址字节大小 (1 或 2)
 * @param  data 接收数据缓冲区指针
 * @param  len 待读取数据字节长度
 * @param  timeout_ms 超时时间限制 (ms)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t port_i2c_mem_read(port_i2c_id_t id, uint8_t dev_addr, uint16_t mem_addr, uint8_t mem_size, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    /* 入口安全防线 */
    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    soft_i2c_t *sw = get_sw(id);
    if (sw != NULL)
    {
        return soft_i2c_mem_read(sw, dev_addr, mem_addr, mem_size, data, len);
    }

    I2C_HandleTypeDef *hw = get_hw(id);
    if (hw != NULL)
    {
        uint16_t hal_mem_size = (mem_size == 1) ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(hw, dev_addr, mem_addr, hal_mem_size, data, len, timeout_ms);
        return (ret == HAL_OK) ? BSP_OK : BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief  I2C 寄存器块写入 (标准融合接口)
 * @param  id I2C 逻辑端口 ID
 * @param  dev_addr 从机 8-bit 地址
 * @param  mem_addr 内部寄存器目标地址
 * @param  mem_size 寄存器地址字节大小 (1 或 2)
 * @param  data 待发送数据缓存指针
 * @param  len 待发送数据字节长度
 * @param  timeout_ms 超时时间限制 (ms)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t port_i2c_mem_write(port_i2c_id_t id, uint8_t dev_addr, uint16_t mem_addr, uint8_t mem_size, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    /* 入口安全防线 */
    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    soft_i2c_t *sw = get_sw(id);
    if (sw != NULL)
    {
        return soft_i2c_mem_write(sw, dev_addr, mem_addr, mem_size, data, len);
    }

    I2C_HandleTypeDef *hw = get_hw(id);
    if (hw != NULL)
    {
        uint16_t hal_mem_size = (mem_size == 1) ? I2C_MEMADD_SIZE_8BIT : I2C_MEMADD_SIZE_16BIT;
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(hw, dev_addr, mem_addr, hal_mem_size, (uint8_t *)data, len, timeout_ms);
        return (ret == HAL_OK) ? BSP_OK : BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief  I2C 原始数据流读取
 * @param  id I2C 逻辑端口 ID
 * @param  dev_addr 从机 8-bit 地址
 * @param  data 接收数据缓冲区指针
 * @param  len 待读取数据字节长度
 * @param  timeout_ms 超时时间限制 (ms)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t port_i2c_read(port_i2c_id_t id, uint8_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    /* 入口安全防线 */
    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    soft_i2c_t *sw = get_sw(id);
    if (sw != NULL)
    {
        return soft_i2c_receive(sw, dev_addr, data, len);
    }

    I2C_HandleTypeDef *hw = get_hw(id);
    if (hw != NULL)
    {
        HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(hw, dev_addr, data, len, timeout_ms);
        return (ret == HAL_OK) ? BSP_OK : BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief  I2C 原始数据流写入
 * @param  id I2C 逻辑端口 ID
 * @param  dev_addr 从机 8-bit 地址
 * @param  data 待发送数据缓存指针
 * @param  len 待发送数据字节长度
 * @param  timeout_ms 超时时间限制 (ms)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t port_i2c_write(port_i2c_id_t id, uint8_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    /* 入口安全防线 */
    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    soft_i2c_t *sw = get_sw(id);
    if (sw != NULL)
    {
        return soft_i2c_transmit(sw, dev_addr, data, len);
    }

    I2C_HandleTypeDef *hw = get_hw(id);
    if (hw != NULL)
    {
        HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hw, dev_addr, (uint8_t *)data, len, timeout_ms);
        return (ret == HAL_OK) ? BSP_OK : BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief  恢复挂死锁死的 I2C 总线
 * @param  id I2C 逻辑端口 ID
 * @retval bsp_status_t 执行结果
 */
bsp_status_t port_i2c_reset_bus(port_i2c_id_t id)
{
    soft_i2c_t *sw = get_sw(id);
    if (sw != NULL)
    {
        soft_i2c_reset_bus(sw);
        return BSP_OK;
    }

    I2C_HandleTypeDef *hw = get_hw(id);
    if (hw != NULL)
    {
        s_hw_i2c_reset_gpio(id);
        return BSP_OK;
    }

    return BSP_EINVAL;
}
