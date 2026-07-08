/**
 * @file port_spi.c
 * @brief SPI 接口层实现
 */

#include "port_spi.h"
#include "spi.h"
#include "bsp_bus.h"
#include <string.h>

/* SPI 异步传输上下文 */
typedef struct
{
    port_async_cb_t callback;
    void            *user_ctx;
    volatile bool   is_busy;
} port_spi_context_t;

static port_spi_context_t s_spi_contexts[PORT_SPI_MAX];

static SPI_HandleTypeDef *hw_mapping[] = {
    [PORT_SPI_1] = &hspi1,
    [PORT_SPI_2] = &hspi2,
};

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

static SPI_HandleTypeDef *get_hw(port_spi_id_t id)
{
    if (id >= PORT_SPI_MAX)
    {
        return NULL;
    }

    if (id == PORT_SPI_2)
    {
        if (bsp_bus_get_mode(BSP_BUS_SPI2_I2S2) != BSP_BUS_MODE_SPI)
        {
            return NULL;
        }
    }

    return hw_mapping[id];
}

static port_spi_id_t get_id_by_handle(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        return PORT_SPI_1;
    }
    else if (hspi == &hspi2)
    {
        return PORT_SPI_2;
    }
    return PORT_SPI_MAX;
}


/* ================================================================
 * 公开接口 API
 * ================================================================ */

/**
 * @brief 初始化指定 SPI 通道
 * @param id SPI 逻辑 ID
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_spi_init(port_spi_id_t id)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL)
    {
        return BSP_ERROR;
    }

    if (HAL_SPI_GetState(hspi) == HAL_SPI_STATE_RESET)
    {
        return BSP_ERROR;
    }

    memset(&s_spi_contexts[id], 0, sizeof(port_spi_context_t));
    return BSP_OK;
}

/**
 * @brief 反初始化并关闭指定 SPI 通道
 * @param id SPI 逻辑 ID
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_spi_deinit(port_spi_id_t id)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL)
    {
        return BSP_ERROR;
    }

    HAL_SPI_DeInit(hspi);
    memset(&s_spi_contexts[id], 0, sizeof(port_spi_context_t));
    return BSP_OK;
}

/**
 * @brief 阻塞模式读取数据（仅支持支持接收的逻辑通道）
 * @param id SPI 逻辑 ID
 * @param data 接收数据缓冲区
 * @param len 期待读取的数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @retval bsp_status_t 成功返回 BSP_OK，通道不支持返回 BSP_EINVAL
 */
bsp_status_t port_spi_read(port_spi_id_t id, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL || data == NULL || len == 0)
    {
        return BSP_ERROR;
    }

    if (HAL_SPI_Receive(hspi, data, len, timeout_ms) != HAL_OK)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}

/**
 * @brief 阻塞模式发送数据
 * @param id SPI 逻辑 ID
 * @param data 待发送数据缓冲区
 * @param len 待发送数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_spi_write(port_spi_id_t id, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL || data == NULL || len == 0)
    {
        return BSP_ERROR;
    }

    if (HAL_SPI_Transmit(hspi, (uint8_t *)data, len, timeout_ms) != HAL_OK)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}

/**
 * @brief 阻塞模式双向同时收发数据（仅支持支持接收的逻辑通道）
 * @brief 使用示例：
 *        uint8_t tx[4] = {0x9F, 0x00, 0x00, 0x00};
 *        uint8_t rx[4] = {0};
 *        port_spi_write_read(PORT_SPI_2, tx, rx, 4, 100);
 * @param id SPI 逻辑 ID
 * @param tx_data 发送数据缓冲区
 * @param rx_data 接收数据缓冲区
 * @param len 传输数据长度
 * @param timeout_ms 超时时间（毫秒）
 * @retval bsp_status_t 成功返回 BSP_OK，通道不支持返回 BSP_EINVAL
 */
bsp_status_t port_spi_write_read(port_spi_id_t id, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL || tx_data == NULL || rx_data == NULL || len == 0)
    {
        return BSP_ERROR;
    }

    if (HAL_SPI_TransmitReceive(hspi, (uint8_t *)tx_data, rx_data, len, timeout_ms) != HAL_OK)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}

/**
 * @brief DMA 模式读取数据（由于物理引脚或 DMA 资源限制，暂安全返回不支持）
 * @param id SPI 逻辑 ID
 * @param data 接收数据缓冲区
 * @param len 传输长度
 * @param cb 传输完成后的回调函数
 * @param user_ctx 用户上下文指针
 * @retval bsp_status_t 始终返回 BSP_EINVAL
 */
bsp_status_t port_spi_read_dma(port_spi_id_t id, uint8_t *data, uint16_t len, port_async_cb_t cb, void *user_ctx)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL || data == NULL || len == 0)
    {
        return BSP_ERROR;
    }

    if (hspi->hdmarx == NULL)
    {
        return BSP_EINVAL;
    }

    if (s_spi_contexts[id].is_busy)
    {
        return BSP_BUSY;
    }

    s_spi_contexts[id].callback = cb;
    s_spi_contexts[id].user_ctx = user_ctx;
    s_spi_contexts[id].is_busy  = true;

    if (HAL_SPI_Receive_DMA(hspi, data, len) != HAL_OK)
    {
        s_spi_contexts[id].is_busy = false;
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief DMA 模式发送数据（仅支持已配置 DMA 的通道）
 * @brief 使用示例：
 *        static uint8_t buffer[128];
 *        void spi_cb(port_spi_id_t id, bsp_status_t res, void *ctx) { ... }
 *        port_spi_write_dma(PORT_SPI_1, buffer, 128, spi_cb, NULL);
 * @param id SPI 逻辑 ID
 * @param data 待发送数据缓冲区
 * @param len 待发送数据长度
 * @param cb 传输完成后的回调函数
 * @param user_ctx 用户上下文指针
 * @retval bsp_status_t 成功提交返回 BSP_OK，忙碌返回 BSP_BUSY，通道不支持返回 BSP_EINVAL
 */
bsp_status_t port_spi_write_dma(port_spi_id_t id, const uint8_t *data, uint16_t len, port_async_cb_t cb, void *user_ctx)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL || data == NULL || len == 0)
    {
        return BSP_ERROR;
    }

    if (hspi->hdmatx == NULL)
    {
        return BSP_EINVAL;
    }

    if (s_spi_contexts[id].is_busy)
    {
        return BSP_BUSY;
    }

    s_spi_contexts[id].callback = cb;
    s_spi_contexts[id].user_ctx = user_ctx;
    s_spi_contexts[id].is_busy  = true;

    if (HAL_SPI_Transmit_DMA(hspi, (uint8_t *)data, len) != HAL_OK)
    {
        s_spi_contexts[id].is_busy = false;
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief DMA 模式双向收发数据（由于物理引脚或 DMA 资源限制，暂安全返回不支持）
 * @param id SPI 逻辑 ID
 * @param tx_data 发送数据缓冲区
 * @param rx_data 接收数据缓冲区
 * @param len 传输长度
 * @param cb 传输完成后的回调函数
 * @param user_ctx 用户上下文指针
 * @retval bsp_status_t 始终返回 BSP_EINVAL
 */
bsp_status_t port_spi_write_read_dma(port_spi_id_t id, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, port_async_cb_t cb, void *user_ctx)
{
    SPI_HandleTypeDef *hspi = get_hw(id);
    if (hspi == NULL || tx_data == NULL || rx_data == NULL || len == 0)
    {
        return BSP_ERROR;
    }

    if (hspi->hdmatx == NULL || hspi->hdmarx == NULL)
    {
        return BSP_EINVAL;
    }

    if (s_spi_contexts[id].is_busy)
    {
        return BSP_BUSY;
    }

    s_spi_contexts[id].callback = cb;
    s_spi_contexts[id].user_ctx = user_ctx;
    s_spi_contexts[id].is_busy  = true;

    if (HAL_SPI_TransmitReceive_DMA(hspi, (uint8_t *)tx_data, rx_data, len) != HAL_OK)
    {
        s_spi_contexts[id].is_busy = false;
        return BSP_ERROR;
    }

    return BSP_OK;
}

/**
 * @brief 查询指定通道是否正处于异步传输中
 * @param id SPI 逻辑 ID
 * @retval bool 忙碌返回 true
 */
bool port_spi_is_busy(port_spi_id_t id)
{
    if (id >= PORT_SPI_MAX)
    {
        return false;
    }
    return s_spi_contexts[id].is_busy;
}

/**
 * @brief 阻塞等待当前通道异步传输完成
 * @param id SPI 逻辑 ID
 * @param timeout_ms 超时时间（毫秒），0 代表无限等待
 * @retval bsp_status_t 成功返回 BSP_OK，超时返回 BSP_ETIMEOUT
 */
bsp_status_t port_spi_wait_complete(port_spi_id_t id, uint32_t timeout_ms)
{
    if (id >= PORT_SPI_MAX)
    {
        return BSP_ERROR;
    }

    uint32_t tickstart = HAL_GetTick();
    while (s_spi_contexts[id].is_busy)
    {
        if (timeout_ms != 0 && (HAL_GetTick() - tickstart >= timeout_ms))
        {
            return BSP_ETIMEOUT;
        }
    }
    return BSP_OK;
}

/* ================================================================
 * HAL SPI 全局中断回调对接
 * ================================================================ */

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    port_spi_id_t id = get_id_by_handle(hspi);
    if (id < PORT_SPI_MAX)
    {
        s_spi_contexts[id].is_busy = false;
        if (s_spi_contexts[id].callback != NULL)
        {
            s_spi_contexts[id].callback((uint8_t)id, BSP_OK, s_spi_contexts[id].user_ctx);
        }
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    port_spi_id_t id = get_id_by_handle(hspi);
    if (id < PORT_SPI_MAX)
    {
        s_spi_contexts[id].is_busy = false;
        if (s_spi_contexts[id].callback != NULL)
        {
            s_spi_contexts[id].callback((uint8_t)id, BSP_OK, s_spi_contexts[id].user_ctx);
        }
    }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    port_spi_id_t id = get_id_by_handle(hspi);
    if (id < PORT_SPI_MAX)
    {
        s_spi_contexts[id].is_busy = false;
        if (s_spi_contexts[id].callback != NULL)
        {
            s_spi_contexts[id].callback((uint8_t)id, BSP_OK, s_spi_contexts[id].user_ctx);
        }
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    port_spi_id_t id = get_id_by_handle(hspi);
    if (id < PORT_SPI_MAX)
    {
        s_spi_contexts[id].is_busy = false;
        if (s_spi_contexts[id].callback != NULL)
        {
            s_spi_contexts[id].callback((uint8_t)id, BSP_ERROR, s_spi_contexts[id].user_ctx);
        }
    }
}
