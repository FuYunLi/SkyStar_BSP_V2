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

/* PWM 映射表，对于暂未在 CubeMX 中使能的通道，其句柄置 NULL */
static const port_pwm_map_t pwm_mapping[PORT_PWM_MAX] =
{
    [PORT_PWM_BUZZER] = {&htim13, TIM_CHANNEL_1},
    [PORT_PWM_WS2812] = {&htim5, TIM_CHANNEL_4},
    [PORT_PWM_LCD_BL] = {&htim10, TIM_CHANNEL_1}
};

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

    /* 动态获取定时器挂载外设的时钟频率 */
    uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
    {
        tim_clk *= 2;
    }

    uint32_t psc = pwm_mapping[pwm].htim->Init.Prescaler + 1;
    uint32_t arr = (tim_clk / psc / freq_hz) - 1;

    __HAL_TIM_SET_AUTORELOAD(pwm_mapping[pwm].htim, arr);

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
