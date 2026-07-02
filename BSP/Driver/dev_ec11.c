/**
 * @file dev_ec11.c
 * @brief EC11 旋转编码器硬件定时器正交解码设备驱动实现
 * @note 封装 TIM4 硬件编码器接口，实时差分计步，消除 CPU 中断开销。
 */

#include "dev_ec11.h"
#include "tim.h"
#include "port_critical.h"

/* ================================================================
 * 静态常量与私有变量
 * ================================================================ */

static int32_t s_ec11_count = 0;        /* 累积计数值 */
static int8_t s_ec11_dir = 0;           /* 旋转方向 */
static uint16_t s_prev_cnt = 0;         /* 上一次读取的计数器值 */
static bool s_is_init = false;          /* 初始化状态指示 */

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 EC11 旋转编码器底层物理引脚与定时器参数
 */
bsp_status_t dev_ec11_init(void)
{
    /* 1. 开启 TIM4 硬件正交解码计数 */
    if (HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return BSP_ERROR;
    }

    /* 2. 读取并记录当前的初始计数值，计数器初始设为0 */
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    s_prev_cnt = 0;
    
    s_ec11_count = 0;
    s_ec11_dir = 0;
    s_is_init = true;

    return BSP_OK;
}

/**
 * @brief 获取当前 EC11 编码器的累积计数值与最后一次旋转方向
 */
bsp_status_t dev_ec11_get_info(dev_ec11_info_t *info)
{
    if (info == NULL)
    {
        return BSP_EINVAL;
    }

    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    uint32_t primask = port_enter_critical();

    /* 1. 差分读取硬件计数器，自动处理硬件溢出 */
    uint16_t curr_cnt = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    int16_t diff = (int16_t)(curr_cnt - s_prev_cnt);
    
    /* 2. 硬件定时器采用 TI1 & TI2 4倍频计数，转动一格计数值改变4 */
    int16_t steps = diff / 4;
    if (steps != 0)
    {
        s_ec11_count += steps;
        s_ec11_dir = (steps > 0) ? 1 : -1;
        s_prev_cnt += (uint16_t)(steps * 4); /* 累加已解析的4倍频计数，保留未转满一格的余数 */
    }

    info->count = s_ec11_count;
    info->dir = s_ec11_dir;

    /* 3. 每次读取完毕后，将最后一次旋转方向清除 */
    s_ec11_dir = 0;

    port_exit_critical(primask);

    return BSP_OK;
}

/**
 * @brief 重置 EC11 旋转编码器的计数值及方向参数
 */
bsp_status_t dev_ec11_reset_count(void)
{
    uint32_t primask = port_enter_critical();

    __HAL_TIM_SET_COUNTER(&htim4, 0);
    s_prev_cnt = 0;
    s_ec11_count = 0;
    s_ec11_dir = 0;

    port_exit_critical(primask);

    return BSP_OK;
}

/**
 * @brief 外部中断处理函数的空实现（向下兼容保留）
 */
void dev_ec11_irq_handler(uint16_t GPIO_Pin)
{
    (void)GPIO_Pin;
}
