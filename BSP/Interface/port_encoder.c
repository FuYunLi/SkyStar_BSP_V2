/**
 * @file port_encoder.c
 * @brief 定时器正交编码器接口层实现
 */

#include "port_encoder.h"
#include "tim.h"
#include "stm32f4xx_hal.h"

/* ================================================================
 * 静态常量与私有变量
 * ================================================================ */

static TIM_HandleTypeDef *s_tim_map[PORT_ENCODER_MAX] = {
    [PORT_ENCODER_EC11] = &htim4,
};

static volatile bool s_encoder_initialized[PORT_ENCODER_MAX] = {false};

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t port_encoder_init(port_encoder_id_t id)
{
    if (id >= PORT_ENCODER_MAX)
    {
        return BSP_EINVAL;
    }

    TIM_HandleTypeDef *htim = s_tim_map[id];
    if (htim == NULL || htim->Instance == NULL)
    {
        return BSP_ERROR;
    }

    s_encoder_initialized[id] = true;
    return BSP_OK;
}

bsp_status_t port_encoder_deinit(port_encoder_id_t id)
{
    if (id >= PORT_ENCODER_MAX)
    {
        return BSP_EINVAL;
    }

    (void)port_encoder_stop(id);
    s_encoder_initialized[id] = false;
    return BSP_OK;
}

bsp_status_t port_encoder_start(port_encoder_id_t id)
{
    if (id >= PORT_ENCODER_MAX)
    {
        return BSP_EINVAL;
    }

    if (!s_encoder_initialized[id])
    {
        return BSP_ERROR;
    }

    TIM_HandleTypeDef *htim = s_tim_map[id];
    if (HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return BSP_ERROR;
    }

    return BSP_OK;
}

bsp_status_t port_encoder_stop(port_encoder_id_t id)
{
    if (id >= PORT_ENCODER_MAX)
    {
        return BSP_EINVAL;
    }

    if (!s_encoder_initialized[id])
    {
        return BSP_ERROR;
    }

    TIM_HandleTypeDef *htim = s_tim_map[id];
    if (HAL_TIM_Encoder_Stop(htim, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return BSP_ERROR;
    }

    return BSP_OK;
}

bsp_status_t port_encoder_set_count(port_encoder_id_t id, uint16_t val)
{
    if (id >= PORT_ENCODER_MAX)
    {
        return BSP_EINVAL;
    }

    if (!s_encoder_initialized[id])
    {
        return BSP_ERROR;
    }

    TIM_HandleTypeDef *htim = s_tim_map[id];
    __HAL_TIM_SET_COUNTER(htim, val);

    return BSP_OK;
}

bsp_status_t port_encoder_get_raw_count(port_encoder_id_t id, uint16_t *val)
{
    if (id >= PORT_ENCODER_MAX || val == NULL)
    {
        return BSP_EINVAL;
    }

    if (!s_encoder_initialized[id])
    {
        return BSP_ERROR;
    }

    TIM_HandleTypeDef *htim = s_tim_map[id];
    *val = (uint16_t)__HAL_TIM_GET_COUNTER(htim);

    return BSP_OK;
}
