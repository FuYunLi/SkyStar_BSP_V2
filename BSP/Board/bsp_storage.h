/**
 * @file bsp_storage.h
 * @brief 板级通用存储服务接口定义
 * @note 应用层仅通过此接口进行数据存储，与具体存储介质型号解耦
 */

#ifndef __BSP_STORAGE_H
#define __BSP_STORAGE_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化板级存储服务
 * @retval bsp_status_t 执行结果，成功返回 BSP_OK
 */
bsp_status_t bsp_storage_init(void);

/**
 * @brief      从存储设备读取数据
 * @param[in]  address 存储起始物理地址
 * @param[out] buf 接收数据缓冲区指针
 * @param[in]  len 待读取数据字节长度
 * @retval     bsp_status_t 执行结果，成功返回 BSP_OK
 */
bsp_status_t bsp_storage_read(uint32_t address, uint8_t *buf, uint32_t len);

/**
 * @brief     向存储设备写入数据
 * @param[in] address 存储起始物理地址
 * @param[in] buf 待写入数据缓存指针
 * @param[in] len 待写入数据字节长度
 * @retval    bsp_status_t 执行结果，成功返回 BSP_OK
 */
bsp_status_t bsp_storage_write(uint32_t address, const uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_STORAGE_H */
