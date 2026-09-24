/**
 * @file bsp_lfs.c
 * @brief LittleFS 板级文件系统适配层
 * @note 负责将 LittleFS 的标准 API 映射到 W25Q128 物理驱动上，使用后 12MB 的空间作为文件系统分区。
 */

#include "bsp_lfs.h"
#include "dev_w25q.h"
#include "bsp_bus.h"
#include "bsp_logger.h"

/* =========================================================================
 * 分区与参数配置
 * ========================================================================= */

/* W25Q128 拥有 16MB (4096 个 4KB 扇区)
 * 物理划分：
 *   - 0~4MB (0~1023 扇区)：预留给中文字库、静态图片直接物理寻址读取。
 *   - 4~16MB (1024~4095 扇区)：挂载 LittleFS 文件系统。
 */
#define LFS_START_BLOCK_OFFSET    1024    /* 文件系统起始逻辑块的物理扇区偏移 */

#define LFS_PORT_BLOCK_SIZE       4096    /* 块大小 = 扇区大小 4KB */
#define LFS_PORT_BLOCK_COUNT      3072    /* 块数量 = 12MB / 4KB */
#define LFS_PORT_READ_SIZE        256     /* 读取块大小 = 页大小 */
#define LFS_PORT_PROG_SIZE        256     /* 写入块大小 = 页大小 */
#define LFS_PORT_CACHE_SIZE       256     /* 缓存大小 = 页大小 (节省 RAM) */
#define LFS_PORT_LOOKAHEAD_SIZE   512     /* 块分配前瞻缓存大小 (位) */
#define LFS_PORT_BLOCK_CYCLES     500     /* 磨损均衡周期 */

/* =========================================================================
 * 私有实例缓冲区及句柄
 * ========================================================================= */

static lfs_t lfs_instance;
static uint8_t lfs_read_buffer[LFS_PORT_CACHE_SIZE];
static uint8_t lfs_prog_buffer[LFS_PORT_CACHE_SIZE];
static uint8_t lfs_lookahead_buffer[LFS_PORT_LOOKAHEAD_SIZE];

/* 最近一次块设备失败原因门闩：LittleFS 本版本无“设备忙”错误码，总线被音频占有
 * 时只能上报 LFS_ERR_IO，导致上层分不清“稍后重试”与“真失败”；此处保留原因供 bsp_file 还原 */
static volatile bsp_status_t s_lfs_last_error = BSP_OK;

/* =========================================================================
 * 辅助映射函数
 * ========================================================================= */

/**
 * @brief 将底层 BSP 状态码映射为 LittleFS 期望的负数错误码
 */
static int bsp_to_lfs_error(bsp_status_t bsp_ret)
{
    switch (bsp_ret)
    {
        case BSP_OK:       return LFS_ERR_OK;       /* 0: 无错误 */
        case BSP_ETIMEOUT: return LFS_ERR_IO;       /* -5: IO 错误 */
        case BSP_EINVAL:   return LFS_ERR_INVAL;    /* -22: 无效参数 */
        case BSP_ENODEV:   return LFS_ERR_IO;       /* -5: 设备异常 */
        default:           return LFS_ERR_IO;       /* 兜底错误 */
    }
}

/**
 * @brief 计算实际的物理地址，包含系统起始偏移
 */
static inline uint32_t get_physical_address(lfs_block_t block, lfs_off_t off)
{
    return ((block + LFS_START_BLOCK_OFFSET) * LFS_PORT_BLOCK_SIZE) + off;
}

/**
 * @brief SPI2 总线仲裁守卫：I2S2 音频持有期间 LittleFS 读写快速失败
 * @note  acquire 在物理侧已为 SPI2 时仅置占用权开销极小；I2S2 持有期间
 *        返回 BSP_BUSY，避免 flash 访问把总线从音频侧抢走或死等。
 */
static bsp_status_t lfs_bus_guard_acquire(void)
{
    bsp_status_t ret = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);

    s_lfs_last_error = ret;
    return ret;
}

static void lfs_bus_guard_release(void)
{
    (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);
}

/* ================================================================
 * 文件系统级重入守卫（需工程宏 LFS_THREADSAFE 才会被 LittleFS 调用）
 *
 * 为什么需要：LVGL 的 F: 盘、各 demo 与开机写 boot.txt 共用同一个 lfs_t 实例，其内部
 * rcache/pcache 与元数据提交状态是有状态的。本工程裸机单线程，正常不会并发；一旦将来
 * 有人在文件操作过程中再次进入文件系统（典型是在渲染/定时器回调里读写文件），后果是
 * 两侧互相踩缓存，表现为难复现的数据不一致。这里把“嵌套”从静默损坏变成显式失败。
 *
 * 写法约束：只做一个标志位，**不关中断**（文件系统操作动辄几十毫秒，关中断会抖断串口
 * 与 I2S 流）；失败时顺手置上总线门闩为 BSP_BUSY，让 bsp_file 能把它还原成可重试语义。
 * ================================================================ */
static volatile bool s_lfs_locked;

static int lfs_lock_cb(const struct lfs_config *c)
{
    (void)c;

    if (s_lfs_locked)
    {
        s_lfs_last_error = BSP_BUSY;   /* 让上层拿到 BSP_BUSY 而不是无法归因的 -1 */
        return LFS_ERR_IO;             /* 本版本无“设备忙”错误码，与块设备回调保持一致 */
    }

    s_lfs_locked = true;
    return LFS_ERR_OK;
}

static int lfs_unlock_cb(const struct lfs_config *c)
{
    (void)c;

    s_lfs_locked = false;
    return LFS_ERR_OK;
}

/* =========================================================================
 * LittleFS 回调实现
 * ========================================================================= */

static int lfs_read_cb(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    (void)c;
    bsp_status_t bus = lfs_bus_guard_acquire();
    if (bus != BSP_OK)
    {
        return bsp_to_lfs_error(bus);
    }
    uint32_t addr = get_physical_address(block, off);
    bsp_status_t ret = dev_w25q_read(addr, buffer, size);
    lfs_bus_guard_release();
    return bsp_to_lfs_error(ret);
}

static int lfs_prog_cb(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    (void)c;
    bsp_status_t bus = lfs_bus_guard_acquire();
    if (bus != BSP_OK)
    {
        return bsp_to_lfs_error(bus);
    }
    uint32_t addr = get_physical_address(block, off);
    bsp_status_t ret = dev_w25q_write(addr, buffer, size);
    lfs_bus_guard_release();
    return bsp_to_lfs_error(ret);
}

static int lfs_erase_cb(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;
    bsp_status_t bus = lfs_bus_guard_acquire();
    if (bus != BSP_OK)
    {
        return bsp_to_lfs_error(bus);
    }
    uint32_t addr = get_physical_address(block, 0);
    bsp_status_t ret = dev_w25q_erase_sector(addr);
    lfs_bus_guard_release();
    return bsp_to_lfs_error(ret);
}

static int lfs_sync_cb(const struct lfs_config *c)
{
    (void)c;
    bsp_status_t bus = lfs_bus_guard_acquire();
    if (bus != BSP_OK)
    {
        return bsp_to_lfs_error(bus);
    }
    bsp_status_t ret = dev_w25q_sync();
    lfs_bus_guard_release();
    return bsp_to_lfs_error(ret);
}

/* =========================================================================
 * LittleFS 挂载配置块
 * ========================================================================= */

static const struct lfs_config lfs_cfg = {
    .context = NULL,
    .read  = lfs_read_cb,
    .prog  = lfs_prog_cb,
    .erase = lfs_erase_cb,
    .sync  = lfs_sync_cb,
    .read_buffer      = lfs_read_buffer,
    .prog_buffer      = lfs_prog_buffer,
    .lookahead_buffer = lfs_lookahead_buffer,
#ifdef LFS_THREADSAFE
    .lock             = lfs_lock_cb,
    .unlock           = lfs_unlock_cb,
#endif
    .block_size       = LFS_PORT_BLOCK_SIZE,
    .block_count      = LFS_PORT_BLOCK_COUNT,
    .read_size        = LFS_PORT_READ_SIZE,
    .prog_size        = LFS_PORT_PROG_SIZE,
    .cache_size       = LFS_PORT_CACHE_SIZE,
    .lookahead_size   = LFS_PORT_LOOKAHEAD_SIZE,
    .block_cycles     = LFS_PORT_BLOCK_CYCLES,
};

/* =========================================================================
 * 导出 API 接口
 * ========================================================================= */

/**
 * @brief 挂载 LittleFS (若未格式化则自动格式化)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t bsp_lfs_mount(void)
{
    static bool is_mounted = false;
    if (is_mounted)
    {
        return BSP_OK;
    }

    /* 1. 先进行底层 Flash 驱动初始化检查（经总线仲裁守卫） */
    bsp_status_t bus = lfs_bus_guard_acquire();
    if (bus != BSP_OK)
    {
        return bus;
    }
    bsp_status_t bsp_ret = dev_w25q_init();
    lfs_bus_guard_release();
    if (bsp_ret != BSP_OK)
    {
        return bsp_ret;
    }

    /* 2. 尝试挂载文件系统 */
    int err = lfs_mount(&lfs_instance, &lfs_cfg);
    if (err != LFS_ERR_OK)
    {
        /* 3. 挂载失败通常是因为未格式化，尝试自动格式化后挂载 */
        err = lfs_format(&lfs_instance, &lfs_cfg);
        if (err == LFS_ERR_OK)
        {
            err = lfs_mount(&lfs_instance, &lfs_cfg);
        }
    }

    if (err == LFS_ERR_OK)
    {
        is_mounted = true;
        return BSP_OK;
    }
    return BSP_ERROR;
}

/**
 * @brief 取回最近一次总线仲裁结果（供 bsp_file 把被压平的 IO 错还原为 BSP_BUSY）
 * @return bsp_status_t BSP_OK 表示上次占有成功；BSP_BUSY 表示总线被其他归属方持有
 */
bsp_status_t bsp_lfs_get_last_error(void)
{
    return s_lfs_last_error;
}

/**
 * @brief 获取挂载成功的 LittleFS 全局实例句柄
 * @return lfs_t* 返回句柄供应用层操作，未挂载成功勿用
 */
lfs_t* bsp_lfs_get_handle(void)
{
    return &lfs_instance;
}
