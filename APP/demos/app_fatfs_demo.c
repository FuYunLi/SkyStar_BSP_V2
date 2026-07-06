/**
 * @file app_fatfs_demo.c
 * @brief FatFS 文件系统功能演示与自检模块源文件
 * @note 导出 fatfs_test 命令到 Letter Shell，提供挂载、卸载、测速和自测等接口
 */

#include "app_fatfs_demo.h"
#include "port_sdio.h"
#include "fatfs.h"
#include "shell.h"
#define LOG_TAG "FATFS_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>

#define TEST_FILE_PATH     "0:/skystar.txt"
#define TEST_BUF_SIZE      4096U  /* 4KB 写入缓冲区用于测速 */

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
        log_e("SDIO physical initialization failed, status: %d", (int)status);
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

int shell_fatfs_test(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_w("Usage: fatfs_test [mount|unmount|info|run]");
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
        HAL_SD_CardInfoTypeDef card_info;
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
    else
    {
        log_w("Unknown sub-command. Usage: fatfs_test [mount|unmount|info|run]");
    }
    
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, fatfs_test, shell_fatfs_test, "FatFS SD card system verification");
