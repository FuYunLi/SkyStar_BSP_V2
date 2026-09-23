/**
 * @file lfs_defines.h
 * @brief LittleFS 移植层覆盖头（通过 -DLFS_DEFINES=lfs_defines.h 注入 lfs_util.h）
 * @note 这是 LittleFS 官方指定的可替换点（见 lfs_util.h 第 29-44 行的用法说明），
 *       因此无需改动 lfs.c / lfs_util.c 任何一行，上游升级可无痛合并。
 *
 *       这里只做一件事：把 LittleFS 的分配器从 C 堆换成 BSP 专用静态池。
 *       本工程 C 堆仅 4 KB（startup Heap_Size）且与 LVGL/shell/日志共用，而 LittleFS
 *       每打开一个文件就要 256 字节（cache_size），并发开文件随时见底，失败只表现为
 *       LFS_ERR_NOMEM，事后极难归因。挂载期的 rcache/pcache/lookahead 已由 bsp_lfs.c
 *       静态提供，不经过这里。
 */

#ifndef __LFS_DEFINES_H
#define __LFS_DEFINES_H

#include "bsp_lfs_pool.h"

#define LFS_MALLOC(sz)  bsp_lfs_pool_alloc(sz)
#define LFS_FREE(p)     bsp_lfs_pool_free(p)

#endif /* __LFS_DEFINES_H */
