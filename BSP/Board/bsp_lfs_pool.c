/**
 * @file bsp_lfs_pool.c
 * @brief LittleFS 专用静态内存池实现
 * @note 设计要点：
 *  1. 固定槽位、无合并：LittleFS 的分配尺寸恒定（cache_size），定长槽最省事也最不易出错；
 *     尺寸不匹配直接失败并报数，绝不越界切分，避免把堆管理类错误带进文件系统层。
 *  2. 槽数 6 覆盖并发打开文件数：LVGL 图片 demo、LVGL 文件系统 demo、bsp_audio 播放、
 *     Ymodem 接收、各 demo 的临时句柄，留一份余量。
 *  3. 本对象被 scatter 点名放入 CCM(0x10000000)：flash 读写全程走轮询 SPI，不经 DMA，
 *     CPU 可直接访问；若将来有人把它改成 DMA，port_spi 新增的 DMA 可达性守卫会显式
 *     返回 BSP_EINVAL，而不是静默搬回垃圾（见 ARCHITECTURE.md 第 4 节第 4 条）。
 *  4. 裸机单线程（主循环轮询）使用，无需原子保护；计数用 volatile 便于调试观察。
 */

#include "bsp_lfs_pool.h"

#define LFS_POOL_SLOTS      (6U)      /* 并发可打开的文件数上限 */
#define LFS_POOL_SLOT_SIZE  (256U)    /* 与 bsp_lfs.c 的 LFS_PORT_CACHE_SIZE 一致 */

/* 以 uint32_t 为底，天然 4 字节对齐；池本体经 scatter 放入 CCM */
static uint32_t s_pool_mem[LFS_POOL_SLOTS][LFS_POOL_SLOT_SIZE / 4U];
static volatile uint8_t s_slot_taken[LFS_POOL_SLOTS];
static volatile uint32_t s_slot_used;      /* 当前占用槽数 */
static volatile uint32_t s_slot_peak;      /* 历史峰值 */
static volatile uint32_t s_alloc_fail;     /* 累计分配失败次数 */

void *bsp_lfs_pool_alloc(size_t size)
{
    if (size == 0U || size > LFS_POOL_SLOT_SIZE)
    {
        s_alloc_fail++;
        return NULL;    /* 用法错误：不切分、不借用邻槽 */
    }

    for (uint32_t i = 0U; i < LFS_POOL_SLOTS; i++)
    {
        if (s_slot_taken[i] == 0U)
        {
            s_slot_taken[i] = 1U;
            s_slot_used++;

            if (s_slot_used > s_slot_peak)
            {
                s_slot_peak = s_slot_used;
            }

            return (void *)s_pool_mem[i];
        }
    }

    s_alloc_fail++;     /* 池耗尽：让 LittleFS 如实返回 NOMEM，而不是悄悄退回 4KB 小堆 */
    return NULL;
}

void bsp_lfs_pool_free(void *p)
{
    if (p == NULL)
    {
        return;
    }

    for (uint32_t i = 0U; i < LFS_POOL_SLOTS; i++)
    {
        if (p == (void *)s_pool_mem[i])
        {
            if (s_slot_taken[i] != 0U)
            {
                s_slot_taken[i] = 0U;
                s_slot_used--;
            }

            return;     /* 只认自己发出的地址，重复释放与外来指针都不破坏池 */
        }
    }
}

void bsp_lfs_pool_stat(uint32_t *peak, uint32_t *fail, uint32_t *slots, uint32_t *slot_size)
{
    if (peak != NULL)
    {
        *peak = s_slot_peak;
    }
    if (fail != NULL)
    {
        *fail = s_alloc_fail;
    }
    if (slots != NULL)
    {
        *slots = LFS_POOL_SLOTS;
    }
    if (slot_size != NULL)
    {
        *slot_size = LFS_POOL_SLOT_SIZE;
    }
}
