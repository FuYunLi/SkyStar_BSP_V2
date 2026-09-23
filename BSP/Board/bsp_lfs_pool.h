/**
 * @file bsp_lfs_pool.h
 * @brief LittleFS 专用静态内存池（替代 C 堆 malloc）
 * @note LittleFS 只在"调用方没提供缓冲"时才分配内存。本仓 v2.11 里唯一的分配点是
 *       lfs.c 打开文件时的 file->cache.buffer（大小 = cfg->cache_size = 256 字节），
 *       挂载期的 rcache/pcache/lookahead 已由 bsp_lfs.c 静态提供，不会走到分配器。
 *       原先这 256 字节来自 C 堆，而本工程 C 堆总共只有 4 KB（startup Heap_Size），
 *       与 LVGL、shell、日志共用，多任务并发开文件随时见底，失败只表现为 LFS_ERR_NOMEM。
 */

#ifndef __BSP_LFS_POOL_H
#define __BSP_LFS_POOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从池中取一块内存
 * @param size 请求字节数，必须不超过单槽容量，否则视为用法错误直接失败
 * @return void* 成功返回槽首地址；池耗尽或尺寸非法返回 NULL（LittleFS 据此报 NOMEM）
 */
void *bsp_lfs_pool_alloc(size_t size);

/**
 * @brief 归还池内存
 * @param p bsp_lfs_pool_alloc 返回的地址；NULL 或非本池地址一律忽略（不崩、不误放）
 */
void bsp_lfs_pool_free(void *p);

/**
 * @brief 取池使用水位，用于板上取证（不看水位就无法证明"真的走了池而没退回堆"）
 * @param peak  输出历史并发占用峰值（槽数），可为 NULL
 * @param fail  输出累计分配失败次数，可为 NULL
 * @param slots 输出槽总数与单槽字节数，可为 NULL
 */
void bsp_lfs_pool_stat(uint32_t *peak, uint32_t *fail, uint32_t *slots, uint32_t *slot_size);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LFS_POOL_H */
