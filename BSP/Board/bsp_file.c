/**
 * @file bsp_file.c
 * @brief 板级虚拟文件系统 (VFS) 胶水层实现
 * @note 支持将文件操作动态路由至 FatFS (SD卡) 或 LittleFS (W25Q128 Flash)
 */

#include "bsp_file.h"
#include "bsp_lfs.h"
#include <string.h>

/* =========================================================================
 * 导出 API 接口
 * ========================================================================= */

/**
 * @brief 打开或创建文件
 */
bsp_status_t bsp_file_open(bsp_file_t *file, const char *path, uint8_t flags)
{
    if (file == NULL || path == NULL)
    {
        return BSP_EINVAL;
    }

    /* 1. 路径路由：以 "0:" 开头则分发至 FatFS (SD卡) */
    if (strncmp(path, "0:", 2) == 0)
    {
        file->type = BSP_FILE_TYPE_FATFS;

        BYTE mode = 0;
        if (flags & BSP_FILE_READ)
        {
            mode |= FA_READ;
        }
        if (flags & BSP_FILE_WRITE)
        {
            mode |= FA_WRITE;
        }
        if (flags & BSP_FILE_CREATE)
        {
            if (flags & BSP_FILE_TRUNC)
            {
                mode |= FA_CREATE_ALWAYS;
            }
            else
            {
                mode |= FA_OPEN_ALWAYS;
            }
        }

        FRESULT fr = f_open(&file->handle.fat_file, path, mode);
        return (fr == FR_OK) ? BSP_OK : BSP_ERROR;
    }
    /* 2. 路径路由：以 "flash/" 开头则分发至 LittleFS (板载 Flash) */
    else if (strncmp(path, "flash/", 6) == 0)
    {
        file->type = BSP_FILE_TYPE_LITTLEFS;

        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int mode = 0;
        if (flags & BSP_FILE_READ)
        {
            mode |= LFS_O_RDONLY;
        }
        if (flags & BSP_FILE_WRITE)
        {
            mode |= LFS_O_WRONLY;
        }
        if (flags & BSP_FILE_CREATE)
        {
            mode |= LFS_O_CREAT;
        }
        if (flags & BSP_FILE_TRUNC)
        {
            mode |= LFS_O_TRUNC;
        }

        /* 剥离 "flash/" 前缀，直接将相对路径传给 LittleFS */
        const char *lfs_path = path + 6;

        int err = lfs_file_open(lfs, &file->handle.lfs_file, lfs_path, mode);
        return (err >= 0) ? BSP_OK : BSP_ERROR;
    }

    file->type = BSP_FILE_TYPE_UNKNOWN;
    return BSP_EINVAL;
}

/**
 * @brief 写入文件数据
 */
bsp_status_t bsp_file_write(bsp_file_t *file, const void *buf, uint32_t len, uint32_t *written)
{
    if (file == NULL || buf == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        UINT bw = 0;
        FRESULT fr = f_write(&file->handle.fat_file, buf, len, &bw);
        if (written != NULL)
        {
            *written = (uint32_t)bw;
        }
        return (fr == FR_OK) ? BSP_OK : BSP_ERROR;
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        lfs_ssize_t res = lfs_file_write(lfs, &file->handle.lfs_file, buf, len);
        if (res >= 0)
        {
            if (written != NULL)
            {
                *written = (uint32_t)res;
            }
            return BSP_OK;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 读取文件数据
 */
bsp_status_t bsp_file_read(bsp_file_t *file, void *buf, uint32_t len, uint32_t *read)
{
    if (file == NULL || buf == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        UINT br = 0;
        FRESULT fr = f_read(&file->handle.fat_file, buf, len, &br);
        if (read != NULL)
        {
            *read = (uint32_t)br;
        }
        return (fr == FR_OK) ? BSP_OK : BSP_ERROR;
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        lfs_ssize_t res = lfs_file_read(lfs, &file->handle.lfs_file, buf, len);
        if (res >= 0)
        {
            if (read != NULL)
            {
                *read = (uint32_t)res;
            }
            return BSP_OK;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 关闭文件并释放资源
 */
bsp_status_t bsp_file_close(bsp_file_t *file)
{
    if (file == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        FRESULT fr = f_close(&file->handle.fat_file);
        file->type = BSP_FILE_TYPE_UNKNOWN;
        return (fr == FR_OK) ? BSP_OK : BSP_ERROR;
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int err = lfs_file_close(lfs, &file->handle.lfs_file);
        file->type = BSP_FILE_TYPE_UNKNOWN;
        return (err >= 0) ? BSP_OK : BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 同步文件数据到物理介质
 */
bsp_status_t bsp_file_sync(bsp_file_t *file)
{
    if (file == NULL)
    {
        return BSP_EINVAL;
    }

    if (file->type == BSP_FILE_TYPE_FATFS)
    {
        FRESULT fr = f_sync(&file->handle.fat_file);
        return (fr == FR_OK) ? BSP_OK : BSP_ERROR;
    }
    else if (file->type == BSP_FILE_TYPE_LITTLEFS)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }

        int err = lfs_file_sync(lfs, &file->handle.lfs_file);
        return (err >= 0) ? BSP_OK : BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 创建单个目录
 */
bsp_status_t bsp_file_mkdir(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }

    if (strncmp(path, "0:", 2) == 0)
    {
        FRESULT fr = f_mkdir(path);
        if (fr == FR_OK || fr == FR_EXIST)
        {
            return BSP_OK;
        }
        return BSP_ERROR;
    }
    else if (strncmp(path, "flash/", 6) == 0)
    {
        lfs_t *lfs = bsp_lfs_get_handle();
        if (lfs == NULL)
        {
            return BSP_ENODEV;
        }
        const char *lfs_path = path + 6;
        int err = lfs_mkdir(lfs, lfs_path);
        if (err >= 0 || err == LFS_ERR_EXIST)
        {
            return BSP_OK;
        }
        return BSP_ERROR;
    }

    return BSP_EINVAL;
}

/**
 * @brief 递归创建目录（若父目录不存在则自动级联创建）
 */
bsp_status_t bsp_file_mkdir_rec(const char *path)
{
    if (path == NULL)
    {
        return BSP_EINVAL;
    }

    char tmp_path[128];
    uint32_t len = strlen(path);
    if (len >= sizeof(tmp_path))
    {
        return BSP_EINVAL;
    }

    strncpy(tmp_path, path, sizeof(tmp_path) - 1);
    tmp_path[sizeof(tmp_path) - 1] = '\0';

    /* 逐层提取目录并创建 */
    char *p = tmp_path;
    
    /* 区分路径类型，跳过根前缀 */
    if (strncmp(tmp_path, "0:", 2) == 0)
    {
        p += 2;
        /* 跳过可能包含的 "0:/" */
        if (*p == '/' || *p == '\\')
        {
            p++;
        }
    }
    else if (strncmp(tmp_path, "flash/", 6) == 0)
    {
        p += 6;
    }
    else
    {
        return BSP_EINVAL;
    }

    /* 循环遍历，遇到分隔符就截断并创建目录 */
    while (*p != '\0')
    {
        if (*p == '/' || *p == '\\')
        {
            char backup = *p;
            *p = '\0';  /* 截断字符串得到当前层级路径 */
            
            bsp_status_t status = bsp_file_mkdir(tmp_path);
            (void)status; /* 忽略错误，因为父目录可能已存在 */
            
            *p = backup; /* 还原字符 */
        }
        p++;
    }

    /* 最后再创建最深的一层目录（如果路径不是以斜杠结尾的话） */
    if (len > 0 && tmp_path[len - 1] != '/' && tmp_path[len - 1] != '\\')
    {
        (void)bsp_file_mkdir(tmp_path);
    }

    return BSP_OK;
}
