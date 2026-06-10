/**
 * @file app_flash_demo.c
 * @brief Flash 与 LittleFS 文件系统演示与自检模块
 */

#include "app_flash_demo.h"
#include "bsp_logger.h"
#include "dev_w25q.h"
#include "bsp_lfs.h"
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
 */
static void shell_flash_id(void)
{
    uint32_t id = 0;
    bsp_status_t ret = dev_w25q_get_id(&id);
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
