/**
 * @file port_can.c
 * @brief CAN 接口层实现
 * @note bxCAN 位时序（APB1 42MHz）：BRP=6 → tq = 166.7ns，
 *       1+BS1(9)+BS2(2) = 12 tq → 500kbps 精确。
 *       环回模式下 bxCAN 在内部将发送帧馈入接收 FIFO，
 *       不依赖收发器与总线，适合无对端自测。
 *       【未验证】CAN 总线尚未实测。
 */

#include "port_can.h"
#include "stm32f4xx_hal.h"

/* ================================================================
 * 私有变量
 * ================================================================ */

/* CAN1 自持句柄：CubeMX 未使能该外设，由本模块自行初始化 */
static CAN_HandleTypeDef s_can1_handle;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 获取 CAN 逻辑通道句柄，未初始化返回 NULL
 */
static CAN_HandleTypeDef *s_can_get_ctx(port_can_id_t id)
{
    if (id >= PORT_CAN_MAX)
    {
        return NULL;
    }

    if (s_can1_handle.Instance != CAN1)
    {
        return NULL;
    }

    return &s_can1_handle;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 CAN 接口
 */
bsp_status_t port_can_init(port_can_id_t id, bool loopback)
{
    if (id >= PORT_CAN_MAX)
    {
        return BSP_EINVAL;
    }

    GPIO_InitTypeDef gpio_init = {0};
    CAN_FilterTypeDef filter = {0};

    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    /* PD0=CAN1_RX、PD1=CAN1_TX，复用推挽 */
    gpio_init.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio_init.Alternate = GPIO_AF9_CAN1;
    (void)HAL_GPIO_Init(GPIOD, &gpio_init);

    s_can1_handle.Instance = CAN1;
    s_can1_handle.Init.Prescaler = 6U;                 /* 42MHz/6 → 7MHz tq */
    s_can1_handle.Init.Mode = loopback ? CAN_MODE_LOOPBACK : CAN_MODE_NORMAL;
    s_can1_handle.Init.SyncJumpWidth = CAN_SJW_1TQ;
    s_can1_handle.Init.TimeSeg1 = CAN_BS1_9TQ;
    s_can1_handle.Init.TimeSeg2 = CAN_BS2_2TQ;
    s_can1_handle.Init.TimeTriggeredMode = DISABLE;
    s_can1_handle.Init.AutoBusOff = DISABLE;
    s_can1_handle.Init.AutoWakeUp = DISABLE;
    s_can1_handle.Init.AutoRetransmission = ENABLE;
    s_can1_handle.Init.ReceiveFifoLocked = DISABLE;
    s_can1_handle.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&s_can1_handle) != HAL_OK)
    {
        return BSP_ERROR;
    }

    /* 过滤器组 0 全开放：所有 ID 直通 FIFO0 */
    filter.FilterBank = 0U;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0x0000U;
    filter.FilterIdLow = 0x0000U;
    filter.FilterMaskIdHigh = 0x0000U;
    filter.FilterMaskIdLow = 0x0000U;
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14U;

    if (HAL_CAN_ConfigFilter(&s_can1_handle, &filter) != HAL_OK)
    {
        return BSP_ERROR;
    }

    if (HAL_CAN_Start(&s_can1_handle) != HAL_OK)
    {
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief 反初始化 CAN
 */
bsp_status_t port_can_deinit(port_can_id_t id)
{
    CAN_HandleTypeDef *hcan = s_can_get_ctx(id);

    if ((id >= PORT_CAN_MAX) || (hcan == NULL))
    {
        return BSP_EINVAL;
    }

    (void)HAL_CAN_Stop(hcan);

    if (HAL_CAN_DeInit(hcan) != HAL_OK)
    {
        return BSP_ERROR;
    }

    s_can1_handle.Instance = NULL;

    return BSP_OK;
}

/**
 * @brief 发送标准数据帧
 */
bsp_status_t port_can_send(port_can_id_t id, const port_can_frame_t *frame)
{
    CAN_HandleTypeDef *hcan = s_can_get_ctx(id);

    if ((hcan == NULL) || (frame == NULL) || (frame->dlc > 8U))
    {
        return BSP_EINVAL;
    }

    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U)
    {
        return BSP_ERROR;
    }

    CAN_TxHeaderTypeDef tx_header = {0};
    tx_header.StdId = frame->id;
    tx_header.IDE = CAN_ID_STD;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = frame->dlc;
    tx_header.TransmitGlobalTime = DISABLE;

    uint32_t mailbox = 0U;

    if (HAL_CAN_AddTxMessage(hcan, &tx_header, (uint8_t *)frame->data, &mailbox) != HAL_OK)
    {
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief 轮询接收一帧
 */
bsp_status_t port_can_poll_recv(port_can_id_t id, port_can_frame_t *frame)
{
    CAN_HandleTypeDef *hcan = s_can_get_ctx(id);

    if ((hcan == NULL) || (frame == NULL))
    {
        return BSP_EINVAL;
    }

    CAN_RxHeaderTypeDef rx_header = {0};
    uint8_t data[8] = {0};

    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, data) != HAL_OK)
    {
        return BSP_ENODEV;
    }

    frame->id = rx_header.StdId;
    frame->dlc = (uint8_t)rx_header.DLC;

    for (uint32_t i = 0U; i < frame->dlc; i++)
    {
        frame->data[i] = data[i];
    }

    return BSP_OK;
}
