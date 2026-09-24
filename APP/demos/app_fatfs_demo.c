/**
 * @file app_fatfs_demo.c
 * @brief FatFS 文件系统功能演示与自检模块源文件
 * @note 导出 fatfs_test 命令到 Letter Shell，提供挂载、卸载、测速和自测等接口
 */

#include "app_fatfs_demo.h"
#include "port_sdio.h"
#include "bsp_file.h"
#include "fatfs.h"
#include "sd_diskio.h"   /* 仅取 DMA 缓冲守卫的拒绝计数，用以区分"被拒"与"数据错" */
#include "shell.h"
#define LOG_TAG "FATFS_DEMO"
#include "elog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE_PATH     "0:/skystar.txt"
#define TEST_BUF_SIZE      4096U  /* 4KB 写入缓冲区用于测速 */

/* 取证工具参数：文件 hexdump 与 SDIO DMA 对齐 A/B 探针 */
#define DUMP_LINE_BYTES    (16U)
#define DUMP_DEF_BYTES     (64U)
#define DUMP_MAX_BYTES     (512U)
#define ALIGN_TEST_PATH    "0:/align.bin"
#define ALIGN_TEST_LEN     (2048U)    /* 两个包，覆盖文件头所在扇区 */
#define ALIGN_TEST_CHUNK   (1024U)    /* 与 Ymodem STX 包载荷等大 */
#define ALIGN_SKEW_BYTES   (3U)       /* 复现 frame_buf[3] 的 4n+3 地址 */
#define ALIGN_SHOW_MAX     (16U)      /* 最多列出前 16 个失配偏移 */
#define ALIGN_DIFF_BYTES   (64U)      /* 失配时打印的 src/rb 对照字节数 */
#define CRC_WIN_BYTES      (64U)      /* 全文件 CRC32 的读窗大小 */
#define CRC_MAP_DEF_BLOCK  (4096U)    /* crcmap 默认分块大小（与 LittleFS block 对齐） */
#define CRC_MAP_MAX_BLOCK  (64U)      /* 最多输出 64 块，防止超长文件刷屏 */
#define CRC32_POLY_REVERSED (0xEDB88320U)  /* zlib/PKZIP 反射多项式，与 PC 侧 binascii.crc32 一致 */

/* 静态文件系统挂载标记 */
static volatile bool s_fs_mounted = false;

/* 测试写入数据 */
static uint8_t s_test_write_buf[TEST_BUF_SIZE];
static uint8_t s_test_read_buf[TEST_BUF_SIZE];

/* 静态函数前缀 s_，符合命名规范 */
static void s_fill_test_buffer(void)
{
    for (uint32_t i = 0; i < TEST_BUF_SIZE; ++i)
    {
        s_test_write_buf[i] = (uint8_t)(i & 0xFF);
    }
}

bsp_status_t app_fatfs_demo_init(void)
{
    log_i("Initializing SDIO Storage Subsystem...");
    
    /* 1. 初始化底层 SDIO 与物理卡检测 */
    bsp_status_t status = port_sdio_init();
    if (status == BSP_ENODEV)
    {
        log_w("SD Card is not detected physically. Auto-mount skipped.");
        s_fs_mounted = false;
        return BSP_ENODEV;
    }
    else if (status != BSP_OK)
    {
        /* 诊断：取接口层翻译后的逻辑错误码，各位含义见 port_sdio.h 的 PORT_SDIO_ERR_* */
        log_e("SDIO physical initialization failed, status: %d, SDIO error: 0x%08lX",
              (int)status, (unsigned long)port_sdio_get_error());
        s_fs_mounted = false;
        return BSP_ERROR;
    }
    
    /* 2. 自动挂载文件系统 */
    FRESULT fr = f_mount(&SDFatFS, (const TCHAR*)SDPath, 1);
    if (fr != FR_OK)
    {
        log_e("FatFS mount failed, error code: %d", (int)fr);
        s_fs_mounted = false;
        return BSP_ERROR;
    }
    
    s_fs_mounted = true;
    log_i("SD Card mounted successfully (Drive: %s).", SDPath);
    return BSP_OK;
}

bsp_status_t app_fatfs_test_run(void)
{
    FIL file;
    UINT bw = 0;
    UINT br = 0;
    FRESULT fr;
    uint32_t t_start, t_end;
    uint32_t write_time, read_time;
    
    if (!s_fs_mounted)
    {
        log_e("FatFS has not been mounted yet. Run 'fatfs_test mount' first.");
        return BSP_ENODEV;
    }
    
    log_i("--- Start FatFS R/W & Speed Test ---");
    s_fill_test_buffer();
    
    /* 1. 写入测试 (4KB) */
    log_i("Creating and writing file: %s (%d bytes)...", TEST_FILE_PATH, TEST_BUF_SIZE);
    t_start = bsp_tick_get_ms();
    fr = f_open(&file, TEST_FILE_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
        log_e("f_open write failed, error: %d", (int)fr);
        return BSP_EIO;
    }
    
    fr = f_write(&file, s_test_write_buf, TEST_BUF_SIZE, &bw);
    f_close(&file);
    t_end = bsp_tick_get_ms();
    
    if (fr != FR_OK || bw != TEST_BUF_SIZE)
    {
        log_e("f_write failed, error: %d, written: %d", (int)fr, (int)bw);
        return BSP_EIO;
    }
    
    write_time = t_end - t_start;
    log_i("Write complete. Duration: %d ms, Speed: %d KB/s", 
      (int)write_time, 
      (int)(TEST_BUF_SIZE / (write_time ? write_time : 1)));
      
    /* 2. 读取测试 (4KB) */
    log_i("Opening and reading file: %s...", TEST_FILE_PATH);
    memset(s_test_read_buf, 0, sizeof(s_test_read_buf));
    t_start = bsp_tick_get_ms();
    fr = f_open(&file, TEST_FILE_PATH, FA_READ);
    if (fr != FR_OK)
    {
        log_e("f_open read failed, error: %d", (int)fr);
        return BSP_EIO;
    }
    
    fr = f_read(&file, s_test_read_buf, TEST_BUF_SIZE, &br);
    f_close(&file);
    t_end = bsp_tick_get_ms();
    
    if (fr != FR_OK || br != TEST_BUF_SIZE)
    {
        log_e("f_read failed, error: %d, read: %d", (int)fr, (int)br);
        return BSP_EIO;
    }
    
    read_time = t_end - t_start;
    log_i("Read complete. Duration: %d ms, Speed: %d KB/s", 
          (int)read_time, 
          (int)(TEST_BUF_SIZE / (read_time ? read_time : 1)));
          
    /* 3. 数据一致性校验 */
    if (memcmp(s_test_write_buf, s_test_read_buf, TEST_BUF_SIZE) == 0)
    {
        log_i("Verification SUCCESS: data matches perfectly!");
    }
    else
    {
        log_e("Verification FAILED: read data mismatch!");
        return BSP_ERROR;
    }
    
    /* 4. 清理测试文件 */
    f_unlink(TEST_FILE_PATH);
    log_i("Temporary test file %s deleted.", TEST_FILE_PATH);
    return BSP_OK;
}

/* =========================================================================
 * 取证工具：文件 hexdump 与 SDIO DMA 对齐 A/B 探针
 * ========================================================================= */

/* 两个缓冲均以 uint32_t 数组为底，保证起始地址天然 4 字节对齐；
 * 写侧再手工 +3 偏出非对齐地址，读侧始终保持对齐。
 * 读回采用 16 字节窗口逐块比对，仅留头部 ALIGN_DIFF_BYTES 字节供对照，
 * 避免为了取证长占整块读回缓冲（本探针对 192KB RAM 的板子仍是负担） */
static uint32_t s_align_pool[ALIGN_TEST_LEN / 4U + ALIGN_SKEW_BYTES];
static uint32_t s_probe_win[DUMP_LINE_BYTES / 4U + 1U];
static uint8_t  s_probe_rb[ALIGN_DIFF_BYTES];

/**
 * @brief 打印一段“源数据 vs 读回数据”对照 hexdump，用于判定损坏形态
 * @param src   写入源指针
 * @param len   输出字节数（按 16 字节一行）
 * @note  三列一组：偏移 / 源 / 读回，便于肉眼区分“移位”与“未写入”
 */
static void app_fatfs_diff_dump(const uint8_t *src, uint32_t len)
{
    const uint8_t *rb = s_probe_rb;

    for (uint32_t off = 0; off < len; off += DUMP_LINE_BYTES)
    {
        printf("%04lX src ", (unsigned long)off);
        for (uint32_t i = 0; i < DUMP_LINE_BYTES; i++)
        {
            printf("%02X ", src[off + i]);
        }
        printf("\r\n%04lX rb  ", (unsigned long)off);
        for (uint32_t i = 0; i < DUMP_LINE_BYTES; i++)
        {
            printf("%02X ", rb[off + i]);
        }
        printf("| ");
        for (uint32_t i = 0; i < DUMP_LINE_BYTES; i++)
        {
            printf("%c", (rb[off + i] == src[off + i]) ? '=' : 'X');
        }
        printf("\r\n");
    }
}

/**
 * @brief 单轮“写入-读回-比对”探针
 * @param src     写入源指针（允许非 4 字节对齐）
 * @param tag     本轮标签
 * @param len     写入总字节数
 * @param chunk   每次写调用的字节数（1024 与 Ymodem 单包等大，以触发整扇区直写）
 * @param gap_ms  打开与首次写之间的空距，用于验证“上一笔写未完”类假设
 * @param via_vfs true=经 bsp_file VFS（非对齐应由 L2 自动中转）；false=直接 f_write（非对齐应由 L1 拒绝）
 * @return int 失配字节数；-1 表示写入被拒或读写流程失败（对直写非对齐轮次，-1 才是期望结果）
 * @note  读回固定 16 字节分块 + 对齐缓冲，走 FatFS 窗口路径，读侧可信；
 *       比对不符时打印前 64 字节 src/rb 对照，直接看出损坏形态
 */
static int app_fatfs_align_probe(const uint8_t *src, const char *tag, uint32_t len, uint32_t chunk,
                                uint32_t gap_ms, bool via_vfs)
{
    FIL file;
    uint32_t bad_list[ALIGN_SHOW_MAX];
    uint32_t off = 0;
    uint32_t shown = 0;
    uint32_t bad_cnt = 0;
    UINT bw = 0;
    UINT br = 0;

    printf("%-22s src=%08lX mod4=%lu len=%lu via=%s : ", tag, (unsigned long)(uintptr_t)src,
           (unsigned long)((uint32_t)(uintptr_t)src & 3U), (unsigned long)len, via_vfs ? "vfs" : "raw");

    if (via_vfs)
    {
        bsp_file_t vf;
        uint32_t vn = 0;

        if (bsp_file_open(&vf, ALIGN_TEST_PATH, BSP_FILE_WRITE | BSP_FILE_CREATE | BSP_FILE_TRUNC) != BSP_OK)
        {
            printf("vfs open(W) FAILED\r\n");
            return -1;
        }
        if (gap_ms != 0U)
        {
            bsp_tick_delay_ms(gap_ms);
        }
        while (off < len)
        {
            uint32_t n = ((len - off) > chunk) ? chunk : (len - off);

            if (bsp_file_write(&vf, src + off, n, &vn) != BSP_OK || vn != n)
            {
                printf("bsp_file_write FAILED at %lu\r\n", (unsigned long)off);
                (void)bsp_file_close(&vf);
                return -1;
            }
            off += n;
        }
        (void)bsp_file_close(&vf);
    }
    else
    {
        if (f_open(&file, ALIGN_TEST_PATH, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
        {
            printf("open(W) FAILED\r\n");
            return -1;
        }
        if (gap_ms != 0U)
        {
            bsp_tick_delay_ms(gap_ms);
        }
        while (off < len)
        {
            uint32_t n = ((len - off) > chunk) ? chunk : (len - off);
            if (f_write(&file, src + off, n, &bw) != FR_OK || bw != n)
            {
                printf("f_write FAILED at %lu\r\n", (unsigned long)off);
                f_close(&file);
                return -1;
            }
            off += n;
        }
        f_close(&file);
    }

    memset(s_probe_rb, 0, sizeof(s_probe_rb));
    if (f_open(&file, ALIGN_TEST_PATH, FA_READ) != FR_OK)
    {
        printf("open(R) FAILED\r\n");
        return -1;
    }
    for (off = 0; off < len; off += DUMP_LINE_BYTES)
    {
        uint8_t *win = (uint8_t *)s_probe_win;
        uint32_t n = ((len - off) > DUMP_LINE_BYTES) ? DUMP_LINE_BYTES : (len - off);

        memset(win, 0, sizeof(s_probe_win));
        if (f_read(&file, win, n, &br) != FR_OK || br != n)
        {
            printf("f_read FAILED at %lu\r\n", (unsigned long)off);
            f_close(&file);
            return -1;
        }

        /* 逐块比对：头部范围内留副本供形态对照，同时累计失配偏移 */
        for (uint32_t i = 0; i < n; i++)
        {
            if ((off + i) < ALIGN_DIFF_BYTES)
            {
                s_probe_rb[off + i] = win[i];
            }
            if (win[i] != src[off + i])
            {
                if (shown < ALIGN_SHOW_MAX)
                {
                    bad_list[shown] = off + i;
                }
                shown++;
                bad_cnt++;
            }
        }
    }
    f_close(&file);
    (void)f_unlink(ALIGN_TEST_PATH);

    printf("mismatches=%lu", (unsigned long)bad_cnt);
    if (bad_cnt != 0U)
    {
        uint32_t limit = (shown < ALIGN_SHOW_MAX) ? shown : ALIGN_SHOW_MAX;
        printf("  bad@");
        for (uint32_t i = 0; i < limit; i++)
        {
            printf("%s%lu", (i == 0U) ? "" : ",", (unsigned long)bad_list[i]);
        }
        if (bad_cnt > limit)
        {
            printf("...");
        }
        printf("\r\n");
        app_fatfs_diff_dump(src, ALIGN_DIFF_BYTES);
    }
    else
    {
        printf("\r\n");
    }
    return (int)bad_cnt;
}

/* 单轮探针的两种期望：逐字节正确，或当场被拒（两者都是“成功”，含义完全不同） */
#define ALIGN_EXPECT_CLEAN     (0)
#define ALIGN_EXPECT_REJECTED  (-1)

/**
 * @brief 比对单轮结果与期望，打印 PASS/FAIL 并返回累计失败数（0 或 1）
 * @note  不能用“没报错”当通过标准：非对齐直写过去是“静默写坏”，现在必须是“报错”，
 *       所以期望要显式区分“报错=对”与“写坏=错”
 */
static int app_fatfs_expect(int got, int expect)
{
    bool ok = (got == expect);

    printf("     => %-4s  expect=%s  got=%d\r\n", ok ? "PASS" : "FAIL",
           (expect == ALIGN_EXPECT_REJECTED) ? "rejected-by-L1" : "byte-clean", got);
    return ok ? 0 : 1;
}

/**
 * @brief DMA 对齐契约双向矩阵：L1 拒绝与 L2 中转各自独立可验证
 * @note  每轮只变一个变量。A 组对齐直写需逐字节正确；U 组非对齐直写需被当场拒绝
 *        （过去它是静默写坏，这条才是修复的真凭据）；V 组同一非对齐指针经 VFS 需正确落盘。
 *        末行给出两个计数器增量作客观证据，避免“看着对了其实是碰巧对齐”
 */
static void app_fatfs_align_test(void)
{
    uint8_t *base = (uint8_t *)s_align_pool;
    uint32_t reject0 = SD_GetBufRejectCount();
    uint32_t stage0 = bsp_file_get_align_stage_count();
    int r;
    int fails = 0;

    if (!s_fs_mounted)
    {
        log_e("File system is not mounted. Run 'fatfs_test mount' first.");
        return;
    }

    for (uint32_t i = 0; i < sizeof(s_align_pool); i++)
    {
        base[i] = (uint8_t)(i * 7U + 3U);
    }

    printf("--- DMA alignment contract matrix (pattern = i*7+3) ---\r\n");

    /* A 组：对齐缓冲直写 FatFS，期望逐字节一致 */
    r = app_fatfs_align_probe(base, "A1 align 512x1", 512U, 512U, 0U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);
    r = app_fatfs_align_probe(base, "A2 align 1024x1", 1024U, 1024U, 0U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);
    r = app_fatfs_align_probe(base, "A3 align 2048x1024", ALIGN_TEST_LEN, ALIGN_TEST_CHUNK, 0U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);
    r = app_fatfs_align_probe(base, "A4 align 2048x1", ALIGN_TEST_LEN, ALIGN_TEST_LEN, 0U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);
    r = app_fatfs_align_probe(base, "A5 align +100ms gap", ALIGN_TEST_LEN, ALIGN_TEST_CHUNK, 100U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);

    /* U 组：非对齐指针直写，期望被 L1 守卫拒绝（不再静默写坏） */
    r = app_fatfs_align_probe(base + ALIGN_SKEW_BYTES, "U1 skew3 1024x1", 1024U, 1024U, 0U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_REJECTED);
    r = app_fatfs_align_probe(base + 1U, "U2 skew1 1024x1", 1024U, 1024U, 0U, false);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_REJECTED);

    /* V 组：同一非对齐指针经 VFS，期望由 L2 中转后逐字节正确 */
    r = app_fatfs_align_probe(base + ALIGN_SKEW_BYTES, "V1 skew3 via-vfs", 1024U, 1024U, 0U, true);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);
    r = app_fatfs_align_probe(base + ALIGN_SKEW_BYTES, "V2 skew3 2048x1024", ALIGN_TEST_LEN,
                              ALIGN_TEST_CHUNK, 0U, true);
    fails += app_fatfs_expect(r, ALIGN_EXPECT_CLEAN);

    (void)f_unlink(ALIGN_TEST_PATH);

    printf("--- rejects=+%lu (L1)  staged=+%lu (L2)  failures=%d  =>  %s ---\r\n",
           (unsigned long)(SD_GetBufRejectCount() - reject0),
           (unsigned long)(bsp_file_get_align_stage_count() - stage0),
           fails, (fails == 0) ? "PASS" : "FAIL");
}

/**
 * @brief 按 16 字节一行打印文件内容，用于与源文件字节级比对
 * @param path   文件路径，支持 VFS 两个后端："0:/..."(SD/FatFS) 与 "flash/..."(W25Q/LittleFS)
 * @param offset 起始偏移（VFS 无 seek，按 16 字节向下对齐后顺序跳读）
 * @param len    输出字节数
 * @note  排版表用 printf（日志规范：对齐表禁用 log_x）；改走 bsp_file VFS 后，
 *       同一命令可对 SD 与 Flash 交叉对账；每行 16 字节，小于扇区，不触发多块传输
 */
static void app_fatfs_dump(const char *path, uint32_t offset, uint32_t len)
{
    static uint32_t s_dump_pool[DUMP_LINE_BYTES / 4U + 1U];
    uint8_t *line = (uint8_t *)s_dump_pool;
    uint32_t align_mask = (uint32_t)DUMP_LINE_BYTES - 1U;
    bsp_file_t file;
    uint32_t base = offset & ~align_mask;
    uint32_t pos = 0;
    uint32_t done = 0;
    uint32_t rd = 0;

    if (bsp_file_open(&file, path, BSP_FILE_READ) != BSP_OK)
    {
        log_e("dump: open %s failed", path);
        return;
    }

    printf("dump %s  (request offset %lu -> start at %lu)\r\n", path, (unsigned long)offset, (unsigned long)base);
    printf("offset      00 01 02 03 04 05 06 07  08 09 10 11 12 13 14 15  | ascii\r\n");

    while (done < len)
    {
        memset(line, 0, sizeof(s_dump_pool));
        if (bsp_file_read(&file, line, DUMP_LINE_BYTES, &rd) != BSP_OK || rd == 0U)
        {
            break;
        }

        if (pos < base)
        {
            pos += rd;
            continue;
        }

        printf("%08lX  ", (unsigned long)pos);
        for (uint32_t i = 0; i < DUMP_LINE_BYTES; i++)
        {
            printf("%02X ", line[i]);
            if (i == 7U)
            {
                printf(" ");
            }
        }
        printf("| ");
        for (uint32_t i = 0; i < rd; i++)
        {
            printf("%c", (line[i] >= 0x20U && line[i] < 0x7FU) ? (char)line[i] : '.');
        }
        printf("\r\n");

        pos += rd;
        done += rd;
        if (rd < DUMP_LINE_BYTES)
        {
            break;
        }
    }
    (void)bsp_file_close(&file);
}

/**
 * @brief CRC32（反射多项式 0xEDB88320）逐字节迭代
 */
static uint32_t s_crc32_update(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (uint8_t i = 0; i < 8U; i++)
    {
        crc = ((crc & 1U) != 0U) ? ((crc >> 1) ^ CRC32_POLY_REVERSED) : (crc >> 1);
    }
    return crc;
}

/**
 * @brief 分块 CRC32 地图：全文件只扫两遍，逐字节路由到各块 CRC，输出损坏分布
 * @param path  文件路径，支持 "0:/..." 与 "flash/..."
 * @param block 分块大小（字节）
 * @note  两遍扫描结果同列输出，用途有二：① 与 PC 侧同块 CRC 对比，一次定位损坏块；
 *       ② pass1 与 pass2 自比可区分“介质内容真坏”与“读路径报数据”：两遍不一致说明读不可靠，
 *       两遍一致但与 PC 不同则落盘确实坏了。不做“每块从头读到区间”，否则全文件要重复读几 MB
 */
static void app_fatfs_crc_map(const char *path, uint32_t block)
{
    /* 读窗以 uint32_t 为底：本层读窗会直达存储后端的 DMA（SDIO/SPI），必须 4 字节对齐 */
    static uint32_t s_win_pool[CRC_WIN_BYTES / 4U];
    static uint32_t s_pass_crc[2][CRC_MAP_MAX_BLOCK];
    uint8_t *s_win = (uint8_t *)s_win_pool;
    bsp_file_t file;
    uint32_t size = 0;
    uint32_t nblk;
    uint32_t rd = 0;
    uint32_t shown;

    if (block < CRC_WIN_BYTES)
    {
        log_w("crcmap: block too small, fallback to %u bytes", (unsigned)CRC_MAP_DEF_BLOCK);
        block = CRC_MAP_DEF_BLOCK;
    }
    if (bsp_file_open(&file, path, BSP_FILE_READ) != BSP_OK)
    {
        log_e("crcmap: open %s failed", path);
        return;
    }
    (void)bsp_file_size(&file, &size);
    (void)bsp_file_close(&file);

    nblk = (size + block - 1U) / block;
    if (nblk > CRC_MAP_MAX_BLOCK)
    {
        log_e("crcmap: %lu blocks exceeds cap %u, enlarge block", (unsigned long)nblk, (unsigned)CRC_MAP_MAX_BLOCK);
        return;
    }

    printf("--- crcmap %s size=%lu block=%lu blocks=%lu ---\r\n", path, (unsigned long)size,
           (unsigned long)block, (unsigned long)nblk);

    for (uint32_t pass = 0; pass < 2U; pass++)
    {
        uint32_t pos = 0;

        for (uint32_t i = 0; i < nblk; i++)
        {
            s_pass_crc[pass][i] = 0xFFFFFFFFU;
        }

        if (bsp_file_open(&file, path, BSP_FILE_READ) != BSP_OK)
        {
            log_e("crcmap: open(pass%u) failed", (unsigned)(pass + 1U));
            return;
        }
        while (bsp_file_read(&file, s_win, CRC_WIN_BYTES, &rd) == BSP_OK && rd != 0U)
        {
            for (uint32_t i = 0; i < rd; i++)
            {
                s_pass_crc[pass][pos / block] = s_crc32_update(s_pass_crc[pass][pos / block], s_win[i]);
                pos++;
            }
        }
        (void)bsp_file_close(&file);

        if (pos < size)
        {
            log_e("crcmap: pass%u stopped early at %lu/%lu", (unsigned)(pass + 1U), (unsigned long)pos,
                  (unsigned long)size);
        }
        for (uint32_t i = 0; i < nblk; i++)
        {
            s_pass_crc[pass][i] ^= 0xFFFFFFFFU;
        }
    }

    printf("blk offset    crc32(pass1) crc32(pass2) same\r\n");
    for (shown = 0; shown < nblk; shown++)
    {
        printf("%3lu %08lX  %08lX      %08lX        %c\r\n", (unsigned long)shown,
               (unsigned long)(shown * block), (unsigned long)s_pass_crc[0][shown],
               (unsigned long)s_pass_crc[1][shown],
               (s_pass_crc[0][shown] == s_pass_crc[1][shown]) ? '=' : '!');
    }
}

/**
 * @brief 经 VFS 读完整文件并算 CRC32，用于与 PC 侧逐字节对账
 * @param path 文件路径，支持 "0:/..." 与 "flash/..."
 * @note  PC 侧等价算法：python -c "import binascii;print('%08x'%binascii.crc32(open(r'文件','rb').read()))"
 *       同一文件经不同后端落盘后比 CRC，可判定数据在哪一段链路被替换。
 *       必须区分“读到 EOF 正常结束”与“读失败中途断开”：CRC 只有在 total==size 时才可信，
 *       所以这里显式报出文件声明长度、实际读到长度与中断位置，不允许静默截断
 */
static void app_fatfs_crc(const char *path)
{
    /* 读窗以 uint32_t 为底：本层读窗会直达存储后端的 DMA（SDIO/SPI），必须 4 字节对齐 */
    static uint32_t s_crc_win_pool[CRC_WIN_BYTES / 4U];
    uint8_t *s_crc_win = (uint8_t *)s_crc_win_pool;
    bsp_status_t st;
    bsp_file_t file;
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t size = 0;
    uint32_t total = 0;
    uint32_t rd = 0;
    uint32_t shortread = 0;
    bool truncated = false;

    st = bsp_file_open(&file, path, BSP_FILE_READ);
    if (st != BSP_OK)
    {
        /* 带上状态码：才能区分 -5(BSP_BUSY，总线被音频持有，可重试) 与 -1/-6(真失败) */
        log_e("crc: open %s failed (status=%d)", path, (int)st);
        return;
    }
    (void)bsp_file_size(&file, &size);

    while (total < size)
    {
        st = bsp_file_read(&file, s_crc_win, CRC_WIN_BYTES, &rd);
        if (st != BSP_OK)
        {
            truncated = true;
            log_e("crc: read ERROR at %lu (status=%d)", (unsigned long)total, (int)st);
            break;
        }
        if (rd == 0U)
        {
            truncated = (total < size);
            if (truncated)
            {
                log_e("crc: read returned 0 at %lu, expected %lu", (unsigned long)total, (unsigned long)size);
            }
            break;
        }
        /* 短读必须单独计数：过去只查 rd==0，使短读被当作正常推进而当成“内容不同”，会误导向 */
        if ((rd != CRC_WIN_BYTES) && ((total + rd) < size))
        {
            shortread++;
            log_w("crc: SHORT read=%lu at %lu", (unsigned long)rd, (unsigned long)total);
        }
        for (uint32_t i = 0; i < rd; i++)
        {
            crc = s_crc32_update(crc, s_crc_win[i]);
        }
        total += rd;
    }
    (void)bsp_file_close(&file);

    printf("%-28s size=%-8lu read=%-8lu crc32=%08lX short=%lu%s\r\n", path, (unsigned long)size,
           (unsigned long)total, (unsigned long)(crc ^ 0xFFFFFFFFU), (unsigned long)shortread,
           truncated ? "  <READ FAILED, crc NOT trustworthy>" : "");
}

int shell_fatfs_test(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_w("Usage: fatfs_test [mount|unmount|info|run|align|rm <path>]");
        log_w("                  [crc <path>|crcmap <path> [blk]|dump <path> [offset] [len]]");
        return -1;
    }
    
    if (strcmp(argv[1], "mount") == 0)
    {
        if (s_fs_mounted)
        {
            log_i("FatFS already mounted.");
            return 0;
        }
        bsp_status_t status = app_fatfs_demo_init();
        if (status != BSP_OK)
        {
            log_e("FatFS mount failed.");
            return -1;
        }
    }
    else if (strcmp(argv[1], "unmount") == 0)
    {
        if (!s_fs_mounted)
        {
            log_i("FatFS has not been mounted.");
            return 0;
        }
        FRESULT fr = f_mount(NULL, (const TCHAR*)SDPath, 0);
        if (fr != FR_OK)
        {
            log_e("Unmount failed, error: %d", (int)fr);
            return -1;
        }
        s_fs_mounted = false;
        log_i("SD Card unmounted successfully.");
    }
    else if (strcmp(argv[1], "info") == 0)
    {
        FATFS *fs;
        DWORD fre_clust, fre_sect, tot_sect;
        
        if (!s_fs_mounted)
        {
            log_e("File system is not mounted. Run 'fatfs_test mount' first.");
            return -1;
        }
        
        /* 1. 获取 FatFS 文件系统容量 */
        FRESULT fr = f_getfree((const TCHAR*)SDPath, &fre_clust, &fs);
        if (fr != FR_OK)
        {
            log_e("f_getfree failed, error: %d", (int)fr);
            return -1;
        }
        
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        
        uint32_t total_mb = tot_sect / 2048U; /* (sectors * 512) / (1024 * 1024) */
        uint32_t free_mb = fre_sect / 2048U;
        
        log_i("--- FatFS Capacity Report ---");
        log_i("File System Type: FAT%d", (fs->fs_type == FS_FAT12) ? 12 : 
                                         ((fs->fs_type == FS_FAT16) ? 16 : 32));
        log_i("Total Space:      %d MB", (int)total_mb);
        log_i("Free Space:       %d MB", (int)free_mb);
        
        /* 2. 获取底层 SD 卡物理参数 */
        port_sdio_card_info_t card_info;
        bsp_status_t status = port_sdio_get_card_info(&card_info);
        if (status == BSP_OK)
        {
            log_i("--- Physical SD Card Details ---");
            log_i("Card Type:        %d", (int)card_info.CardType);
            log_i("Card Version:     %d", (int)card_info.CardVersion);
            log_i("Block Size:       %d bytes", (int)card_info.BlockSize);
            log_i("Block Number:     %d", (int)card_info.BlockNbr);
            log_i("Card Capacity:    %d MB", (int)(((uint64_t)card_info.BlockNbr * card_info.BlockSize) / (1024 * 1024)));
        }
    }
    else if (strcmp(argv[1], "run") == 0)
    {
        app_fatfs_test_run();
    }
    else if (strcmp(argv[1], "align") == 0)
    {
        app_fatfs_align_test();
    }
    else if (strcmp(argv[1], "crc") == 0)
    {
        if (argc < 3)
        {
            log_w("Usage: fatfs_test crc <path>   (path prefix: 0:/ or flash/)");
            return -1;
        }
        app_fatfs_crc(argv[2]);
    }
    else if (strcmp(argv[1], "crcmap") == 0)
    {
        uint32_t blk = CRC_MAP_DEF_BLOCK;

        if (argc < 3)
        {
            log_w("Usage: fatfs_test crcmap <path> [block]");
            return -1;
        }
        if (argc >= 4)
        {
            blk = (uint32_t)strtoul(argv[3], NULL, 10);
        }
        app_fatfs_crc_map(argv[2], blk);
    }
    else if (strcmp(argv[1], "dump") == 0)
    {
        uint32_t off = 0;
        uint32_t len = DUMP_DEF_BYTES;

        if (argc < 3)
        {
            log_w("Usage: fatfs_test dump <path> [offset] [len]");
            return -1;
        }
        if (argc >= 4)
        {
            off = (uint32_t)strtoul(argv[3], NULL, 10);
        }
        if (argc >= 5)
        {
            len = (uint32_t)strtoul(argv[4], NULL, 10);
        }
        if (len > DUMP_MAX_BYTES)
        {
            log_w("dump length clamped to %lu bytes", (unsigned long)DUMP_MAX_BYTES);
            len = DUMP_MAX_BYTES;
        }
        app_fatfs_dump(argv[2], off, len);
    }
    else if (strcmp(argv[1], "rm") == 0)
    {
        /* VFS 删除的验收入口：两后端通用，同时把映射后的状态码直接报出来，
         * 以区分"不存在"(BSP_ENODEV) 与 "真失败"(BSP_ERROR/BSP_EIO 等) */
        bsp_status_t st;

        if (argc < 3)
        {
            log_w("Usage: fatfs_test rm <path>   (path prefix: 0:/ or flash/)");
            return -1;
        }
        st = bsp_file_remove(argv[2]);
        if (st == BSP_OK)
        {
            log_i("Removed %s", argv[2]);
        }
        else
        {
            log_e("rm %s failed (status=%d)", argv[2], (int)st);
            return -1;
        }
    }
    else
    {
        log_w("Unknown sub-command. Usage: fatfs_test [mount|unmount|info|run|align|crc|crcmap|dump|rm]");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, fatfs_test, shell_fatfs_test, "FatFS SD card system verification");
