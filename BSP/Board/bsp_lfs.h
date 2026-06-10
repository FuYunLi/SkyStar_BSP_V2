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

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LFS_H */
