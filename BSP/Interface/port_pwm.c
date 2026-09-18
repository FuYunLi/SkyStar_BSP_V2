/**
 * @file port_pwm.c
 * @brief PWM 接口层实现
 * @note 封装底层定时器的 PWM 功能，隔离 HAL 库并提供统一状态码返回。
 */

#include "port_pwm.h"
#include "tim.h"

typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t channel;
} port_pwm_map_t;

/* ================================================================
 * 私有变量
 * ================================================================ */

/* 舵机通道自持定时器句柄：TIM12 未在 CubeMX 使能，由本模块自行初始化，
 * 等效于 CubeMX 生成代码（时钟/GPIO AF9/时基/双通道 PWM1） */
static TIM_HandleTypeDef s_tim12_handle;

/* 舵机时基：TIM12 输入 84MHz（APB1 定时器时钟），PSC=83 → 1MHz 计数，
 * ARR=19999 → 20ms 周期（50Hz），CCR 值即脉冲宽度微秒数 */
#define PWM_TIM12_PSC (83U)
#define PWM_TIM12_ARR (19999U)

/* PWM 映射表，对于暂未在 CubeMX 中使能的通道，其句柄置 NULL */
static const port_pwm_map_t pwm_mapping[PORT_PWM_MAX] =
{
    [PORT_PWM_BUZZER] = {&htim13, TIM_CHANNEL_1},
    [PORT_PWM_WS2812] = {&htim5, TIM_CHANNEL_4},
    [PORT_PWM_LCD_BL] = {&htim10, TIM_CHANNEL_1},
    [PORT_PWM_SERVO1] = {&s_tim12_handle, TIM_CHANNEL_1},
    [PORT_PWM_SERVO2] = {&s_tim12_handle, TIM_CHANNEL_2}
};

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 初始化舵机通道定时器 TIM12（等效 CubeMX 生成代码）
 * @note 硬件连接：PB14=TIM12_CH1（舵机1），PB15=TIM12_CH2（舵机2），
 *       复用功能 AF9；SW7 拨码 BIT5 需置于"双舵机"位。
 * @retval BSP_OK 初始化成功
 */
static bsp_status_t s_pwm_tim12_init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    TIM_MasterConfigTypeDef master_config = {0};
    TIM_OC_InitTypeDef oc_config = {0};

    __HAL_RCC_TIM12_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* CH1/CH2 引脚复用推挽，PWM 输出无外部上拉需求 */
    gpio_init.Pin = GPIO_PIN_14 | GPIO_PIN_15;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = GPIO_AF9_TIM12;
    (void)HAL_GPIO_Init(GPIOB, &gpio_init);

    s_tim12_handle.Instance = TIM12;
    s_tim12_handle.Init.Prescaler = PWM_TIM12_PSC;
    s_tim12_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    s_tim12_handle.Init.Period = PWM_TIM12_ARR;
    s_tim12_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    s_tim12_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&s_tim12_handle) != HAL_OK)
    {
        return BSP_ERROR;
    }

    master_config.MasterOutputTrigger = TIM_TRGO_RESET;
    master_config.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&s_tim12_handle, &master_config) != HAL_OK)
    {
        return BSP_ERROR;
    }

    /* 两通道同配置：PWM1 模式，比较值 0（初始无脉冲输出） */
    oc_config.OCMode = TIM_OCMODE_PWM1;
    oc_config.Pulse = 0U;
    oc_config.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc_config.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&s_tim12_handle, &oc_config, TIM_CHANNEL_1) != HAL_OK)
    {
        return BSP_ERROR;
    }
    if (HAL_TIM_PWM_ConfigChannel(&s_tim12_handle, &oc_config, TIM_CHANNEL_2) != HAL_OK)
    {
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief 获取定时器实例的实际输入时钟频率
 * @note 硬件原理：TIM1/TIM8/TIM9-TIM11 挂载 APB2，其余定时器挂载 APB1。
 *       当所在总线分频不为 1 时，定时器时钟为总线时钟的 2 倍（参考手册
 *       时钟树：APB 分频非 1 时定时器时钟倍频）。
 *       此前实现写死 APB1 逻辑，导致 APB2 定时器（如 TIM10 背光通道）
 *       计算频率偏差一倍，本函数按实例地址归属总线动态判定。
 * @param htim 定时器句柄
 * @return uint32_t 定时器输入时钟（Hz）
 */
static uint32_t s_pwm_get_timer_clk(const TIM_HandleTypeDef *htim)
{
    uint32_t tim_clk;

    if ((uint32_t)htim->Instance >= APB2PERIPH_BASE)
    {
        tim_clk = HAL_RCC_GetPCLK2Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1)
        {
            tim_clk *= 2U;
        }
    }
    else
    {
        tim_clk = HAL_RCC_GetPCLK1Freq();
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
        {
            tim_clk *= 2U;
        }
    }

    return tim_clk;
}

/**
 * @brief 初始化检测逻辑通道
 * @param pwm PWM 逻辑通道 ID
 * @retval BSP_OK 检测通过，通道就绪
 * @retval BSP_EINVAL 输入 ID 越界
 * @retval BSP_ERROR 底层句柄未初始化
 */
bsp_status_t port_pwm_init(port_pwm_id_t pwm)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    /* 舵机通道使用未经 CubeMX 初始化的 TIM12，首次调用时自持初始化 */
    if ((pwm == PORT_PWM_SERVO1) || (pwm == PORT_PWM_SERVO2))
    {
        if (s_tim12_handle.Instance != TIM12)
        {
            bsp_status_t ret = s_pwm_tim12_init();
            if (ret != BSP_OK)
            {
                return ret;
            }
        }
    }

    return BSP_OK;
}

/**
 * @brief 开启 PWM 通道输出
 * @param pwm PWM 逻辑通道 ID
 * @retval BSP_OK 启动成功
 * @retval BSP_EINVAL 输入 ID 越界
 * @retval BSP_ERROR 底层句柄未初始化
 */
bsp_status_t port_pwm_start(port_pwm_id_t pwm)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    HAL_StatusTypeDef ret = HAL_TIM_PWM_Start(pwm_mapping[pwm].htim, pwm_mapping[pwm].channel);
    return hal_to_bsp_status(ret);
}

/**
 * @brief 关闭 PWM 通道输出
 * @param pwm PWM 逻辑通道 ID
 * @retval BSP_OK 停止成功
 * @retval BSP_EINVAL 输入 ID 越界
 * @retval BSP_ERROR 底层句柄未初始化
 */
bsp_status_t port_pwm_stop(port_pwm_id_t pwm)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    HAL_StatusTypeDef ret = HAL_TIM_PWM_Stop(pwm_mapping[pwm].htim, pwm_mapping[pwm].channel);
    return hal_to_bsp_status(ret);
}

/**
 * @brief 设置 PWM 通道占空比
 * @note 依据参考手册，通过修改捕获/比较寄存器 (TIMx_CCRx) 写入脉宽比较值。
 *       比较值计算公式：CCR = (ARR + 1) * duty_permille / 1000。
 * @param pwm PWM 逻辑通道 ID
 * @param duty_permille 占空比千分比 (0 - 1000)
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 输入 ID 越界或占空比无效
 * @retval BSP_ERROR 底层句柄未初始化
 *
 * 示例：
 *   port_pwm_set_duty(PORT_PWM_BUZZER, 500); // 设置蜂鸣器占空比为 50%
 */
bsp_status_t port_pwm_set_duty(port_pwm_id_t pwm, uint16_t duty_permille)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    if (duty_permille > 1000)
    {
        duty_permille = 1000;
    }

    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pwm_mapping[pwm].htim);
    uint32_t ccr = (arr + 1) * duty_permille / 1000;
    __HAL_TIM_SET_COMPARE(pwm_mapping[pwm].htim, pwm_mapping[pwm].channel, ccr);

    return BSP_OK;
}

/**
 * @brief 动态调节 PWM 通道频率
 * @note 依据参考手册，修改自动重装载寄存器 (TIMx_ARR) 的值调节周期。
 *       ARR 计算公式：ARR = F_clk / (F_target * (PSC + 1)) - 1。
 *       对于挂载在 APB 总线上的定时器，若其总线分频系数不为 1，则输入时钟自动乘 2。
 * @param pwm PWM 逻辑通道 ID
 * @param freq_hz 目标频率（Hz）
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 输入 ID 越界或频率为 0
 * @retval BSP_ERROR 底层句柄未初始化
 *
 * 示例：
 *   port_pwm_set_freq(PORT_PWM_BUZZER, 2000); // 设置蜂鸣器 PWM 频率为 2000Hz
 */
bsp_status_t port_pwm_set_freq(port_pwm_id_t pwm, uint32_t freq_hz)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    if (freq_hz == 0)
    {
        return BSP_EINVAL;
    }

    /* 动态获取定时器挂载总线的时钟频率（APB1/APB2 自动判定） */
    uint32_t tim_clk = s_pwm_get_timer_clk(pwm_mapping[pwm].htim);

    uint32_t psc = pwm_mapping[pwm].htim->Init.Prescaler + 1U;
    /* 四舍五入减小整除截断引入的频率偏差 */
    uint32_t arr = ((tim_clk / psc) + freq_hz / 2U) / freq_hz - 1U;

    __HAL_TIM_SET_AUTORELOAD(pwm_mapping[pwm].htim, arr);
    /* 同步句柄缓存，保持 Init 字段与寄存器实况一致 */
    pwm_mapping[pwm].htim->Init.Period = arr;

    return BSP_OK;
}

/**
 * @brief 获取 PWM 通道的自动重装载寄存器 (ARR) 当前值
 * @param pwm PWM 逻辑通道 ID
 * @return uint32_t 当前自动重装载寄存器的值
 */
uint32_t port_pwm_get_arr(port_pwm_id_t pwm)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return 0;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return 0;
    }

    return __HAL_TIM_GET_AUTORELOAD(pwm_mapping[pwm].htim);
}

/**
 * @brief 开启 PWM 通道的 DMA 传输模式 (常用于 WS2812 级联驱动等)
 * @param pwm PWM 逻辑通道 ID
 * @param data 待发送的 DMA 数据缓冲区指针
 * @param len 传输数据长度
 * @retval BSP_OK 启动成功
 * @retval BSP_EINVAL 参数无效或指针为空
 * @retval BSP_ERROR 底层句柄未初始化
 */
bsp_status_t port_pwm_dma_start(port_pwm_id_t pwm, void *data, uint16_t len)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    HAL_StatusTypeDef ret = HAL_TIM_PWM_Start_DMA(pwm_mapping[pwm].htim, pwm_mapping[pwm].channel, (uint32_t *)data, len);
    return hal_to_bsp_status(ret);
}

/**
 * @brief 停止 PWM 通道的 DMA 传输模式
 * @param pwm PWM 逻辑通道 ID
 * @retval BSP_OK 停止成功
 * @retval BSP_EINVAL 输入 ID 越界
 * @retval BSP_ERROR 底层句柄未初始化
 */
bsp_status_t port_pwm_dma_stop(port_pwm_id_t pwm)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    HAL_StatusTypeDef ret = HAL_TIM_PWM_Stop_DMA(pwm_mapping[pwm].htim, pwm_mapping[pwm].channel);
    return hal_to_bsp_status(ret);
}

/**
 * @brief 直接设置 PWM 通道比较值（脉冲宽度）
 * @note 舵机通道专用便捷接口：TIM12 时基为 1MHz 计数（见 s_pwm_tim12_init），
 *       比较值即脉冲宽度微秒数，典型范围 500-2500µs 对应 0-180°。
 *       其他通道的时基并非 1µs，勿混用本接口。
 * @param pwm PWM 逻辑通道 ID（限 PORT_PWM_SERVO1/SERVO2）
 * @param pulse_us 脉冲宽度（微秒）
 * @retval BSP_OK 设置成功
 * @retval BSP_EINVAL 参数无效
 * @retval BSP_ERROR 底层句柄未初始化
 */
bsp_status_t port_pwm_set_pulse_us(port_pwm_id_t pwm, uint16_t pulse_us)
{
    if (pwm >= PORT_PWM_MAX)
    {
        return BSP_EINVAL;
    }

    if ((pwm != PORT_PWM_SERVO1) && (pwm != PORT_PWM_SERVO2))
    {
        return BSP_EINVAL;
    }

    if (pwm_mapping[pwm].htim == NULL)
    {
        return BSP_ERROR;
    }

    /* 脉冲宽度不得超出 ARR 周期 */
    if (pulse_us > (uint16_t)PWM_TIM12_ARR)
    {
        pulse_us = (uint16_t)PWM_TIM12_ARR;
    }

    __HAL_TIM_SET_COMPARE(pwm_mapping[pwm].htim, pwm_mapping[pwm].channel, pulse_us);

    return BSP_OK;
}
