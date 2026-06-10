/**
 * @file app_spi_demo.h
 * @brief SPI 演示与测试模块头文件
 */

#ifndef __APP_SPI_DEMO_H
#define __APP_SPI_DEMO_H

#include "bsp_board.h"

/**
 * @brief 初始化 SPI 演示模块，导出命令到 Shell
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_spi_demo_init(void);

#endif /* __APP_SPI_DEMO_H */
