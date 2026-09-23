/**
 * @file bsp_file.h
 * @brief 板级虚拟文件系统 (VFS) 胶水层
 * @note 统一 FatFS 与 LittleFS 接口，提供平台无关的文件操作 API
 */

#ifndef __BSP_FILE_H
#define __BSP_FILE_H

#include "bsp_board.h"
#include "ff.h"
#include "lfs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    BSP_FILE_TYPE_FATFS = 0,
    BSP_FILE_TYPE_LITTLEFS,
    BSP_FILE_TYPE_UNKNOWN
} bsp_file_type_t;

typedef struct
{
    bsp_file_type_t type;
    union
    {
        FIL fat_file;
        lfs_file_t lfs_file;
    } handle;
} bsp_file_t;

/* 文件打开模式宏定义 */
#define BSP_FILE_READ    (1U << 0)
#define BSP_FILE_WRITE   (1U << 1)
#define BSP_FILE_CREATE  (1U << 2)
#define BSP_FILE_TRUNC   (1U << 3)

/**
 * @brief 打开或创建文件
 * @param file 文件结构体指针
 * @param path 文件路径（"0:/..." 指向 FatFS，"flash/..." 指向 LittleFS）
 * @param flags 打开模式标志位
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_open(bsp_file_t *file, const char *path, uint8_t flags);

/**
 * @brief 写入文件数据
 * @param file 文件结构体指针
 * @param buf 待写入的数据缓冲区
 * @param len 待写入字节数
 * @param written 实际写入字节数指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_write(bsp_file_t *file, const void *buf, uint32_t len, uint32_t *written);

/**
 * @brief 读取文件数据（从当前读写位置继续）
 * @param file 文件结构体指针
 * @param buf 存储读取数据的缓冲区
 * @param len 期待读取的字节数
 * @param readed 实际读取字节数指针（文件尾时小于 len，可为 NULL）
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_read(bsp_file_t *file, void *buf, uint32_t len, uint32_t *readed);

/**
 * @brief 获取文件总大小（字节）
 * @param file 文件结构体指针
 * @param size 存储文件大小的指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_size(bsp_file_t *file, uint32_t *size);

/**
 * @brief 关闭文件并释放资源
 * @param file 文件结构体指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_close(bsp_file_t *file);

/**
 * @brief 同步文件数据到物理介质
 * @param file 文件结构体指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_sync(bsp_file_t *file);

/**
 * @brief 创建单个目录
 * @param path 目录路径（"0:/..." 指向 FatFS，"flash/..." 指向 LittleFS）
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_mkdir(const char *path);

/**
 * @brief 递归创建目录（若父目录不存在则自动级联创建）
 * @param path 完整目录路径
 * @return bsp_status_t 执行结果
 */
bsp_status_t bsp_file_mkdir_rec(const char *path);

/**
 * @brief 删除文件
 * @param path 文件路径（"0:/..." 指向 FatFS，"flash/..." 指向 LittleFS）
 * @return bsp_status_t BSP_OK 已删除；BSP_ENODEV 目标不存在；其余为底层错误
 */
bsp_status_t bsp_file_remove(const char *path);

/**
 * @brief 重命名文件（常用于“临时文件写完后改名”以提交结果）
 * @param old_path 原路径
 * @param new_path 新路径；已存在时先删除再改名，以覆盖语义
 * @return bsp_status_t 执行结果；两路径分属不同后端时返回 BSP_EINVAL
 * @note  不支持跨后端改名（SD <-> Flash），调用方须自行保证同侧
 */
bsp_status_t bsp_file_rename(const char *old_path, const char *new_path);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_FILE_H */
