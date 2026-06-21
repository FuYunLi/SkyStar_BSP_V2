/**
 * @file lv_port_fs.c
 * @brief LVGL 9.3.0 文件系统设备移植层源文件
 */

#include "lv_port_fs.h"
#include "bsp_lfs.h"
#include <stdbool.h>

/**********************
 *  STATIC PROTOTYPES
 * ********************/
static bool fs_ready(lv_fs_drv_t *drv);
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw);
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

/**********************
 *   GLOBAL FUNCTIONS
 * ********************/

/**
 * @brief 初始化 LVGL 文件系统接口并向系统注册文件系统设备
 * @details 负责映射并注册 LittleFS 设备至 'F' 盘符
 */
void lv_port_fs_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);

    /* 映射文件系统挂载的盘符 */
    fs_drv.letter = 'F';

    /* 绑定底层 LittleFS 文件系统驱动的回调 */
    fs_drv.ready_cb = fs_ready;
    fs_drv.open_cb = fs_open;
    fs_drv.close_cb = fs_close;
    fs_drv.read_cb = fs_read;
    fs_drv.write_cb = fs_write;
    fs_drv.seek_cb = fs_seek;
    fs_drv.tell_cb = fs_tell;

    /* 注册文件系统设备 */
    lv_fs_drv_register(&fs_drv);
}

/**********************
 *   STATIC FUNCTIONS
 * ********************/

/* 判断底层物理文件系统是否准备就绪 */
static bool fs_ready(lv_fs_drv_t *drv)
{
    (void)drv;
    return (bsp_lfs_get_handle() != NULL);
}

/* 打开文件回调 */
static void *fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv;
    lfs_t *lfs = bsp_lfs_get_handle();
    if (!lfs)
    {
        return NULL;
    }

    lfs_file_t *file = lv_malloc(sizeof(lfs_file_t));
    if (!file)
    {
        return NULL;
    }

    int flags = 0;
    if (mode == LV_FS_MODE_WR)
    {
        flags = LFS_O_WRONLY | LFS_O_CREAT;
    }
    else if (mode == LV_FS_MODE_RD)
    {
        flags = LFS_O_RDONLY;
    }
    else if (mode == (LV_FS_MODE_WR | LV_FS_MODE_RD))
    {
        flags = LFS_O_RDWR | LFS_O_CREAT;
    }

    int err = lfs_file_open(lfs, file, path, flags);
    if (err < 0)
    {
        lv_free(file);
        return NULL;
    }

    return file;
}

/* 关闭文件回调 */
static lv_fs_res_t fs_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t *file = (lfs_file_t *)file_p;

    if (!lfs || !file)
    {
        return LV_FS_RES_INV_PARAM;
    }

    int err = lfs_file_close(lfs, file);
    lv_free(file);

    return (err < 0) ? LV_FS_RES_UNKNOWN : LV_FS_RES_OK;
}

/* 读取文件回调 */
static lv_fs_res_t fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv;
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t *file = (lfs_file_t *)file_p;

    if (!lfs || !file)
    {
        return LV_FS_RES_INV_PARAM;
    }

    lfs_ssize_t res = lfs_file_read(lfs, file, buf, btr);
    if (res < 0)
    {
        return LV_FS_RES_UNKNOWN;
    }

    *br = (uint32_t)res;
    return LV_FS_RES_OK;
}

/* 写入文件回调 */
static lv_fs_res_t fs_write(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw)
{
    (void)drv;
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t *file = (lfs_file_t *)file_p;

    if (!lfs || !file)
    {
        return LV_FS_RES_INV_PARAM;
    }

    lfs_ssize_t res = lfs_file_write(lfs, file, buf, btw);
    if (res < 0)
    {
        return LV_FS_RES_UNKNOWN;
    }

    *bw = (uint32_t)res;
    return LV_FS_RES_OK;
}

/* 文件指针定位回调 */
static lv_fs_res_t fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t *file = (lfs_file_t *)file_p;

    if (!lfs || !file)
    {
        return LV_FS_RES_INV_PARAM;
    }

    int lfs_whence = LFS_SEEK_SET;
    if (whence == LV_FS_SEEK_CUR)
    {
        lfs_whence = LFS_SEEK_CUR;
    }
    else if (whence == LV_FS_SEEK_END)
    {
        lfs_whence = LFS_SEEK_END;
    }

    lfs_soff_t res = lfs_file_seek(lfs, file, (lfs_soff_t)pos, lfs_whence);
    return (res < 0) ? LV_FS_RES_UNKNOWN : LV_FS_RES_OK;
}

/* 获取文件指针当前位置回调 */
static lv_fs_res_t fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    lfs_t *lfs = bsp_lfs_get_handle();
    lfs_file_t *file = (lfs_file_t *)file_p;

    if (!lfs || !file || !pos_p)
    {
        return LV_FS_RES_INV_PARAM;
    }

    lfs_soff_t res = lfs_file_tell(lfs, file);
    if (res < 0)
    {
        return LV_FS_RES_UNKNOWN;
    }

    *pos_p = (uint32_t)res;
    return LV_FS_RES_OK;
}
