/**
 * @file dev_w25q.h
 * @brief W25Q128 SPI Flash 底层物理驱动头文件
 * @note 提供纯物理读写擦接口，不带缓存，设计为直接挂接 LittleFS 及字库物理直读使用。
 */

#ifndef __DEV_W25Q_H
#define __DEV_W25Q_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* W25Q128 核心参数 */
#define W25Q_PAGE_SIZE      256
#define W25Q_SECTOR_SIZE    4096
#define W25Q_BLOCK_SIZE     65536
#define W25Q_CHIP_SIZE      (16 * 1024 * 1024)

/* 基础 API */
bsp_status_t dev_w25q_init(void);
bsp_status_t dev_w25q_read(uint32_t addr, uint8_t *buf, uint32_t size);
bsp_status_t dev_w25q_write(uint32_t addr, const uint8_t *buf, uint32_t size);
bsp_status_t dev_w25q_erase_sector(uint32_t addr);
bsp_status_t dev_w25q_sync(void);
bsp_status_t dev_w25q_get_id(uint32_t *p_id);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_W25Q_H */
