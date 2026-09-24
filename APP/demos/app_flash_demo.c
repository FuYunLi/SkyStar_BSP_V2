/**
 * @file app_flash_demo.c
 * @brief Flash 与 LittleFS 文件系统演示与自检模块
 */

#include "app_flash_demo.h"
#include "bsp_logger.h"
#include "dev_w25q.h"
#include "bsp_bus.h"
#include "bsp_lfs.h"
#include "bsp_lfs_pool.h"
#include "bsp_file.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BOOT_COUNT_FILE "boot.txt"

/* =========================================================================
 * 辅助函数：启动计数自检
 * ========================================================================= */

static void check_and_update_boot_count(void)
{
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t file;
    uint32_t boot_count = 0;
    int err;

    /* 尝试打开文件读取当前计数 */
    err = lfs_file_open(lfs, &file, BOOT_COUNT_FILE, LFS_O_RDWR | LFS_O_CREAT);
    if (err == LFS_ERR_OK)
    {
        /* 读取计数，如果文件是空的，读取会失败或返回0，做相应处理 */
        char buf[16] = {0};
        lfs_ssize_t read_len = lfs_file_read(lfs, &file, buf, sizeof(buf) - 1);
        if (read_len > 0)
        {
            boot_count = (uint32_t)atoi(buf);
        }
        
        /* 计数累加 */
        boot_count++;
        
        /* 将游标移回文件开头，重写计数 */
        lfs_file_rewind(lfs, &file);
        snprintf(buf, sizeof(buf), "%lu\n", (unsigned long)boot_count);
        lfs_file_write(lfs, &file, buf, strlen(buf));
        
        /* 确保截断尾部多余数据（如果新字符串比旧的短） */
        lfs_file_truncate(lfs, &file, strlen(buf));
        
        lfs_file_close(lfs, &file);
        
        log_i("LittleFS Mount Success. Boot Count: %lu", (unsigned long)boot_count);
    }
    else
    {
        log_e("LittleFS Failed to open/create boot_count file! Error: %d", err);
    }
}

/* =========================================================================
 * 初始化接口
 * ========================================================================= */

/**
 * @brief 初始化 Flash 演示模块并挂载文件系统
 */
bsp_status_t app_flash_demo_init(void)
{
    /* 尝试挂载 LittleFS (若失败则自动格式化后挂载) */
    bsp_status_t ret = bsp_lfs_mount();
    if (ret == BSP_OK)
    {
        /* 更新并打印启动计数 */
        check_and_update_boot_count();
    }
    else
    {
        log_e("LittleFS Mount Failed!");
    }
    
    return ret;
}

/* =========================================================================
 * Shell 指令导出
 * ========================================================================= */

/**
 * @brief 测试指令：读取 Flash JEDEC ID
 * @note  经 bsp_bus 仲裁申请 SPI2：I2S2 音频持有期间快速失败报 BUSY，
 *        而不是在总线已被切走的情况下物理读取（旧版会假死）。
 */
static void shell_flash_id(void)
{
    uint32_t id = 0;
    bsp_status_t bus = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);
    if (bus != BSP_OK)
    {
        log_e("SPI2 bus not available (ret = %d), audio may own the bus", bus);
        return;
    }
    bsp_status_t ret = dev_w25q_get_id(&id);
    (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);
    if (ret == BSP_OK)
    {
        log_i("W25Q128 JEDEC ID: 0x%06X", id);
    }
    else
    {
        log_e("Failed to read Flash ID. ret = %d", ret);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), flash_id, shell_flash_id, Read W25Q128 JEDEC ID);

/**
 * @brief 测试指令：遍历 LittleFS 根目录文件
 */
static void shell_lfs_ls(int argc, char *agrv[])
{
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_dir_t dir;
    struct lfs_info info;
    
    const char *path = "/";
    if (argc > 1)
    {
        path = agrv[1];
    }

    int err = lfs_dir_open(lfs, &dir, path);
    if (err != LFS_ERR_OK)
    {
        log_e("Failed to open dir: %s (err: %d)", path, err);
        return;
    }

    log_i("Directory listing for: %s", path);
    log_i("--------------------------------");
    
    while (lfs_dir_read(lfs, &dir, &info) > 0)
    {
        if (info.type == LFS_TYPE_DIR)
        {
            log_i(" [DIR]  %s", info.name);
        }
        else
        {
            log_i(" [FILE] %s\t\t(%lu Bytes)", info.name, (unsigned long)info.size);
        }
    }
    
    log_i("--------------------------------");
    lfs_dir_close(lfs, &dir);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), lfs_ls, shell_lfs_ls, List LittleFS directory);

/**
 * @brief 测试指令：显示当前的 Boot Count
 */
static void shell_lfs_boot_count(void)
{
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t file;
    int err = lfs_file_open(lfs, &file, BOOT_COUNT_FILE, LFS_O_RDONLY);
    
    if (err == LFS_ERR_OK)
    {
        char buf[16] = {0};
        lfs_file_read(lfs, &file, buf, sizeof(buf) - 1);
        lfs_file_close(lfs, &file);
        log_i("Current Boot Count: %s", buf);
    }
    else
    {
        log_e("Failed to read boot count (err: %d)", err);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), lfs_boot_count, shell_lfs_boot_count, Show current boot count);

/* =========================================================================
 * 取证探针：W25Q 原始读路径的可复现性（不经 LittleFS，撇开文件系统变量）
 * ========================================================================= */

#define FLASH_STABLE_MAX (256U) /* 单次探测长度上限；与 LittleFS cache_size/read_size(256) 对齐，最具代表性 */

/* 两个比较缓冲均以 uint32_t 打底，保证 4 字节对齐（见 ARCHITECTURE 第 4 节契约） */
static uint32_t s_stable_first[FLASH_STABLE_MAX / 4U];
static uint32_t s_stable_next[FLASH_STABLE_MAX / 4U];

/**
 * @brief flash_stable 指令：对同一物理地址持续直读 N 次，量化读路径是否可复现
 * @param argc 参数个数
 * @param argv 参数列表：addr [len [times]]
 * @note  目的是把“SPI/驱动层偶发错”与“LittleFS 层逻辑”两个变量分开：
 *       原始直读可复现 => 嫌疑在文件系统侧；不可复现 => 坐实在驱动/总线侧。
 *       同时检查 dev_w25q_read 返回值是否与实际数据一致（当前实现无论如何都报 OK）
 */
static int shell_flash_stable(int argc, char *argv[])
{
    uint32_t addr = 0U;
    uint32_t len = 256U;
    uint32_t times = 8U;
    uint32_t bad_pass = 0U;
    uint8_t *first = (uint8_t *)s_stable_first;
    uint8_t *next = (uint8_t *)s_stable_next;
    bsp_status_t ret0;

    if (argc >= 2)
    {
        addr = (uint32_t)strtoul(argv[1], NULL, 0);
    }
    if (argc >= 3)
    {
        len = (uint32_t)strtoul(argv[2], NULL, 0);
    }
    if (argc >= 4)
    {
        times = (uint32_t)strtoul(argv[3], NULL, 0);
    }

    if (len == 0U || len > FLASH_STABLE_MAX || times < 2U)
    {
        log_e("Usage: flash_stable <addr> [len<=%u] [times>=2]", (unsigned)FLASH_STABLE_MAX);
        return -1;
    }

    ret0 = dev_w25q_read(addr, first, len);
    printf("flash_stable addr=0x%06lX len=%lu times=%lu ret1=%d\r\n",
           (unsigned long)addr, (unsigned long)len, (unsigned long)times, (int)ret0);

    for (uint32_t p = 2U; p <= times; p++)
    {
        uint32_t diff = 0U;
        uint32_t first_off = 0U;
        bsp_status_t ret = dev_w25q_read(addr, next, len);

        for (uint32_t j = 0; j < len; j++)
        {
            if (next[j] != first[j])
            {
                if (diff == 0U)
                {
                    first_off = j;
                }
                diff++;
            }
        }

        if (diff != 0U)
        {
            bad_pass++;
            printf("  pass%-3lu UNSTABLE ret=%d diff=%lu first=+%lu (0x%06lX)  [%02X vs %02X]\r\n",
                   (unsigned long)p, (int)ret, (unsigned long)diff, (unsigned long)first_off,
                   (unsigned long)(addr + first_off), first[first_off], next[first_off]);
        }
    }

    printf("=> unstable passes: %lu/%lu (ret=%d on every pass means no error is reported)\r\n",
           (unsigned long)bad_pass, (unsigned long)(times - 1U), (int)ret0);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), flash_stable, shell_flash_stable, Probe raw W25Q read repeatability);

/* =========================================================================
 * 取证探针：LittleFS 同命令内多遍读的可复现性 + “另一使用者”因果注入 + 堆水位
 * ========================================================================= */

#define LFS_PROBE_WIN   (256U)   /* 逐块读窗，避免大缓冲占用 RAM */
#define LFS_PROBE_BYTES (8192U)  /* 哈希覆盖长度，足够跨过几个 block */
#define LFS_PROBE_PASSES (8U)

static uint32_t s_probe_win[LFS_PROBE_WIN / 4U];

/**
 * @brief FNV-1a 32 位非加密哈希，仅用于比对多次读结果是否相同
 */
static uint32_t s_fnv1a(const uint8_t *p, uint32_t n, uint32_t h)
{
    for (uint32_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= 16777619U;
    }
    return h;
}

/**
 * @brief 经 VFS 完整读一段文件并算哈希
 * @param name 带前缀的路径（如 "flash/xxx"）
 * @param hash_out 输出哈希
 * @param got_out  输出实际读到字节数（小于 LFS_PROBE_BYTES 即为中途截断）
 * @return bsp_status_t 区分失败类型，用于分开“堆耗尽导致 open 失败”与“真读到不同数据”：
 *         BSP_OK 完整读完；BSP_ENODEV open 失败；BSP_EIO 读中途截断；BSP_ERROR 读报错
 */
static bsp_status_t lfs_probe_hash(const char *name, uint32_t *hash_out, uint32_t *got_out)
{
    uint8_t *win = (uint8_t *)s_probe_win;
    bsp_file_t file;
    uint32_t hash = 2166136261U;
    uint32_t done = 0U;
    uint32_t rd = 0U;
    bsp_status_t st;

    *hash_out = 0U;
    *got_out = 0U;

    if (bsp_file_open(&file, name, BSP_FILE_READ) != BSP_OK)
    {
        return BSP_ENODEV;
    }

    st = BSP_OK;
    while (done < LFS_PROBE_BYTES)
    {
        uint32_t n = ((LFS_PROBE_BYTES - done) > LFS_PROBE_WIN) ? LFS_PROBE_WIN : (LFS_PROBE_BYTES - done);
        bsp_status_t rst = bsp_file_read(&file, win, n, &rd);

        if (rst != BSP_OK)
        {
            st = BSP_ERROR;
            break;
        }
        if (rd == 0U)
        {
            st = BSP_EIO;
            break;
        }
        hash = s_fnv1a(win, rd, hash);
        done += rd;
    }
    if ((st == BSP_OK) && (done != LFS_PROBE_BYTES))
    {
        st = BSP_EIO;
    }
    (void)bsp_file_close(&file);

    *hash_out = hash;
    *got_out = done;
    return st;
}

/**
 * @brief 模拟“另一个使用者”的文件系统活动（与 LVGL 图片目录扫描同类）
 * @return int 0 表示扫描正常完成，负值为 LittleFS 错误码（包括堆不够导致的 NOSPC）
 */
static int lfs_probe_noise(void)
{
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_dir_t dir;
    struct lfs_info info;
    int err;

    if (lfs == NULL)
    {
        return LFS_ERR_IO;
    }
    err = lfs_dir_open(lfs, &dir, "/");
    if (err != LFS_ERR_OK)
    {
        return err;
    }
    while (lfs_dir_read(lfs, &dir, &info) > 0)
    {
        /* 仅遍历，不做其他事 */
    }
    lfs_dir_close(lfs, &dir);
    return 0;
}

/**
 * @brief 二分探测当前堆的最大可连续分配量
 * @note  未定义 LFS_NO_MALLOC，LittleFS 每次 open 都靠 malloc 要文件 cache；
 *        堆不够会直接表现为打开/读失败或中途截断，故必须量一下
 */
static void lfs_probe_heap(void)
{
    uint32_t lo = 0U;
    uint32_t hi = 65536U;

    while ((lo + 512U) < hi)
    {
        uint32_t mid = (lo + hi) / 2U;
        void *p = malloc((size_t)mid);

        if (p != NULL)
        {
            free(p);
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    printf("heap : largest contiguous malloc ok ~ %lu bytes\r\n", (unsigned long)lo);
}

/**
 * @brief lfs_probe 指令：单命令内对照组 + 噪声注入组，分离“并发污染”与“驱动错”
 * @param argc 参数个数
 * @param argv argv[1] = 带前缀路径，默认 flash/Tour_France.wav
 * @note  同一条命令内主循环不会执行其他任务，因此：
 *        A 组（无注入）应完全一致；若 B 组（每遍之间插一次目录扫描）出现不一致，
 *        则因果成立：“另一使用者碰过文件系统”会改变本文件的读结果
 */
static int shell_lfs_probe(int argc, char *argv[])
{
    const char *name = (argc >= 2) ? argv[1] : "flash/Tour_France.wav";
    uint32_t distinct[2] = {1U, 1U};
    uint32_t openfail[2] = {0U, 0U};
    uint32_t shortread[2] = {0U, 0U};
    uint32_t noisefail = 0U;
    uint32_t got = 0U;
    uint32_t prev[2] = {0U, 0U};

    printf("--- lfs_probe %s (%u bytes, %u passes) ---\r\n", name, (unsigned)LFS_PROBE_BYTES,
           (unsigned)LFS_PROBE_PASSES);

    for (uint32_t phase = 0U; phase < 2U; phase++)
    {
        printf("%s:\r\n", (phase == 0U) ? "A no-inject" : "B injected");

        for (uint32_t i = 0U; i < LFS_PROBE_PASSES; i++)
        {
            uint32_t hash = 0U;
            int ne = 0;
            bsp_status_t st = lfs_probe_hash(name, &hash, &got);

            if (st == BSP_ENODEV)
            {
                openfail[phase]++;
            }
            else if (st != BSP_OK)
            {
                shortread[phase]++;
            }
            if ((i != 0U) && (hash != prev[phase]))
            {
                distinct[phase]++;
            }
            prev[phase] = hash;

            if (phase == 1U)
            {
                ne = lfs_probe_noise();
                if (ne != 0)
                {
                    noisefail++;
                }
            }
            printf("  pass%-2lu st=%-3d hash=0x%08lX got=%lu noise=%d\r\n", (unsigned long)(i + 1U),
                   (int)st, (unsigned long)hash, (unsigned long)got, ne);
        }
    }

    printf("=> A: distinct=%lu open_fail=%lu short_read=%lu\r\n", (unsigned long)distinct[0],
           (unsigned long)openfail[0], (unsigned long)shortread[0]);
    printf("=> B: distinct=%lu open_fail=%lu short_read=%lu noise_fail=%lu\r\n", (unsigned long)distinct[1],
           (unsigned long)openfail[1], (unsigned long)shortread[1], (unsigned long)noisefail);

    if ((openfail[0] + openfail[1] + noisefail) != 0U)
    {
        printf("=> VERDICT: open/dir failures present -> LittleFS relying on an exhausted malloc heap is the driver\r\n");
    }
    else if (distinct[0] == 1U && distinct[1] > 1U)
    {
        printf("=> VERDICT: opens all OK but content varies after another user's FS activity -> shared lfs_t state corruption\r\n");
    }
    else if (distinct[0] > 1U)
    {
        printf("=> VERDICT: unstable even without injection -> driver/VFS level, not concurrency\r\n");
    }
    else
    {
        printf("=> VERDICT: stable this run, repeat to confirm\r\n");
    }
    lfs_probe_heap();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), lfs_probe, shell_lfs_probe, Probe LittleFS read determinism and heap headroom);

/* =========================================================================
 * 取证探针：把“提前返回”归因到 LittleFS 还是 flash 驱动
 * ========================================================================= */

/**
 * @brief lfs_diag 指令：并列跑两条路径对同一长度做同样的分块读，比较谁先出错
 * @param argc 参数个数
 * @param argv argv[1]=LFS 文件名（不带 flash/ 前缀），默认 Tour_France.wav
 * @note  A 路径直接调 lfs_file_read（打印原始 lfs_ssize_t，不经 bsp_file 压平），
 *        B 路径用相同 256 分块直接调 dev_w25q_read（不经文件系统）。
 *        若只有 A 错 -> 问题在 LittleFS 层；若 B 也错 -> 问题在 flash 驱动/总线
 */
static int shell_lfs_diag(int argc, char *argv[])
{
    const char *fname = (argc >= 2) ? argv[1] : "Tour_France.wav";
    uint32_t raw_fail = 0U;
    uint32_t drv_fail = 0U;
    lfs_t *lfs = bsp_lfs_get_handle();
    uint8_t *win = (uint8_t *)s_probe_win;

    if (lfs == NULL)
    {
        log_e("lfs_diag: LittleFS not mounted");
        return -1;
    }

    printf("--- lfs_diag %s : %u bytes x %u passes, chunk=%u ---\r\n", fname,
           (unsigned)LFS_PROBE_BYTES, (unsigned)LFS_PROBE_PASSES, (unsigned)LFS_PROBE_WIN);

    /* A：直接走 LittleFS，打印原始返回码 */
    for (uint32_t p = 0U; p < LFS_PROBE_PASSES; p++)
    {
        lfs_file_t file;
        uint32_t done = 0U;
        int err = lfs_file_open(lfs, &file, fname, LFS_O_RDONLY);

        if (err < 0)
        {
            printf("  A pass%-2lu open err=%d\r\n", (unsigned long)(p + 1U), err);
            raw_fail++;
            continue;
        }
        while (done < LFS_PROBE_BYTES)
        {
            lfs_ssize_t r = lfs_file_read(lfs, &file, win, LFS_PROBE_WIN);

            if (r < 0)
            {
                printf("  A pass%-2lu read err=%d at %lu\r\n", (unsigned long)(p + 1U), (int)r,
                       (unsigned long)done);
                raw_fail++;
                break;
            }
            if (r == 0)
            {
                printf("  A pass%-2lu SHORT read=0 at %lu\r\n", (unsigned long)(p + 1U), (unsigned long)done);
                raw_fail++;
                break;
            }
            done += (uint32_t)r;
            if ((uint32_t)r != LFS_PROBE_WIN)
            {
                printf("  A pass%-2lu SHORT read=%ld at %lu\r\n", (unsigned long)(p + 1U), (long)r,
                       (unsigned long)done);
                raw_fail++;
            }
        }
        lfs_file_close(lfs, &file);
    }

    /* B：同样的 256 分块，但裸读 flash（从文件系统数据区起始块试） */
    for (uint32_t p = 0U; p < LFS_PROBE_PASSES; p++)
    {
        uint32_t addr = 0x400000U;   /* LittleFS 分区起点，内容非全 0xFF */

        for (uint32_t c = 0U; c < LFS_PROBE_BYTES / LFS_PROBE_WIN; c++)
        {
            bsp_status_t st = dev_w25q_read(addr + (c * LFS_PROBE_WIN), win, LFS_PROBE_WIN);

            if (st != BSP_OK)
            {
                printf("  B pass%-2lu chunk%-3lu st=%d @0x%06lX\r\n", (unsigned long)(p + 1U),
                       (unsigned long)c, (int)st, (unsigned long)(addr + (c * LFS_PROBE_WIN)));
                drv_fail++;
            }
        }
    }

    printf("=> A(LittleFS) fails=%lu   B(raw flash) fails=%lu\r\n",
           (unsigned long)raw_fail, (unsigned long)drv_fail);
    printf("=> VERDICT: %s\r\n",
           (raw_fail != 0U && drv_fail == 0U) ? "only LittleFS fails -> fault is inside the filesystem layer"
           : (drv_fail != 0U)                 ? "raw flash fails too -> fault is in the driver/bus layer"
                                              : "both clean this run -> repeat, or it is timing/load dependent");
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), lfs_diag, shell_lfs_diag, Localize LittleFS vs raw flash read failure);

/**
 * @brief lfs_pool 指令：查看 LittleFS 专用静态池水位
 * @param argc 参数个数
 * @param argv 参数列表（未使用）
 * @return int 恒为 0
 * @note peak>0 才能证明 LittleFS 真的用了池；fail>0 说明槽数不够，需调大 LFS_POOL_SLOTS；
 *       与 lfs_probe 里的 C 堆水位对照，可看出 4KB 小堆已不再是文件系统的隐性上限
 */
static int shell_lfs_pool(int argc, char *argv[])
{
    uint32_t peak = 0U;
    uint32_t fail = 0U;
    uint32_t slots = 0U;
    uint32_t slot_size = 0U;

    (void)argc;
    (void)argv;

    bsp_lfs_pool_stat(&peak, &fail, &slots, &slot_size);
    printf("lfs pool: %lu slots x %lu B, peak=%lu, alloc_fail=%lu\r\n",
           (unsigned long)slots, (unsigned long)slot_size, (unsigned long)peak, (unsigned long)fail);

    if (peak == 0U)
    {
        printf("  note: peak=0 means LittleFS never allocated (all buffers statically provided)\r\n");
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), lfs_pool, shell_lfs_pool, Show LittleFS static pool watermark);
