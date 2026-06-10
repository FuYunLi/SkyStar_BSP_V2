/**
 * @file port_spi.h
 * @brief SPI 物理层抽象接口头文件
 * @note 遵循 SkyStar BSP V2 规范，隔离 HAL 库，提供统一的多通道 SPI 阻塞与异步传输接口
 */

#ifndef __PORT_SPI_H
#define __PORT_SPI_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    PORT_SPI_1 = 0U,
    PORT_SPI_2,
    PORT_SPI_MAX
} port_spi_id_t;

bsp_status_t port_spi_init(port_spi_id_t id);
bsp_status_t port_spi_deinit(port_spi_id_t id);

bsp_status_t port_spi_read(port_spi_id_t id, uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t port_spi_write(port_spi_id_t id, const uint8_t *data, uint16_t len, uint32_t timeout_ms);
bsp_status_t port_spi_write_read(port_spi_id_t id, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms);

bsp_status_t port_spi_read_dma(port_spi_id_t id, uint8_t *data, uint16_t len, port_async_cb_t cb, void *user_ctx);
bsp_status_t port_spi_write_dma(port_spi_id_t id, const uint8_t *data, uint16_t len, port_async_cb_t cb, void *user_ctx);
bsp_status_t port_spi_write_read_dma(port_spi_id_t id, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, port_async_cb_t cb, void *user_ctx);

bool         port_spi_is_busy(port_spi_id_t id);
bsp_status_t port_spi_wait_complete(port_spi_id_t id, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_SPI_H */
