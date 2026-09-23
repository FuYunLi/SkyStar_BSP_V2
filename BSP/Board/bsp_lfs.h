/**
 * @file bsp_lfs.h
 * @brief LittleFS 板级文件系统适配层头文件
 * @note 负责将 LittleFS 挂载至 W25Q128 闪存特定分区，并提供统一的句柄获取接口。
 */

#ifndef __BSP_LFS_H
#define __BSP_LFS_H

#include "lfs.h"
#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 暴露给外部的全局文件系统句柄 */
lfs_t* bsp_lfs_get_handle(void);

/* 文件系统挂载初始化接口 */
bsp_status_t bsp_lfs_mount(void);

/**
 * @brief 取回最近一次 SPI2 总线仲裁结果
 * @return bsp_status_t BSP_OK 上次占有成功；BSP_BUSY 总线被其他归属方（如 I2S2 音频）持有
 * @note LittleFS 本版本没有"设备忙"错误码，块设备失败会被统一成 LFS_ERR_IO；
 *       bsp_file 依赖本接口把它还原成可重试语义，调用方无须主动清零（下次占有自动刷新）
 */
bsp_status_t bsp_lfs_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LFS_H */
