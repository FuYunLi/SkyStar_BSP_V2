/**
 * @file ota_image.h
 * @brief OTA 镜像头部契约（Bootloader 与 App 共享的唯一权威定义）
 * @note 本文件是 bootloader 与 App 两个工程之间的唯一共享点，
 *       字段增删属于架构决策，须双侧同步并升版本。头部 64B 内
 *       单次页编程（W25Q 页 256B）完成写入，天然原子。
 *       状态机铁律：头部永远最后写；DOWNLOADING/非法 magic 均视为
 *       无效镜像（断电安全由此保证，无需断点续传）。
 */

#ifndef __OTA_IMAGE_H
#define __OTA_IMAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 常量定义
 * ================================================================ */

#define OTA_MAGIC           (0x4F544147UL)  /* "OTAG" 小端 */
#define OTA_W25Q_BASE       (0x00000000UL)  /* OTA 区起始于 W25Q 首地址 */
#define OTA_HEADER_ADDR     (OTA_W25Q_BASE) /* 头部固定于区首（独占 4KB 扇区） */
#define OTA_IMAGE_ADDR      (0x00004000UL)  /* 镜像数据起始于 16KB 对齐处：
                                            头部独占扇区 0，状态机每次更新 =
                                            擦头扇区 + 重编程（W25Q 页编程
                                            只能 1→0，无法原地翻转状态位） */
#define OTA_REGION_SIZE     (0x400000UL)    /* OTA 区 4MB */
#define OTA_IMAGE_MAX       (480U * 1024U)  /* 镜像上限 = App 区大小 */

/* 镜像状态机 */
#define OTA_STATE_EMPTY       (0x00000000UL) /* 区已擦除/无有效内容 */
#define OTA_STATE_DOWNLOADING (0x00000001UL) /* 传输中（bootloader 忽略） */
#define OTA_STATE_READY       (0x00000002UL) /* CRC 已校验，待拷贝 */
#define OTA_STATE_JUMPED      (0x00000003UL) /* 已拷贝到片上并跳转 */
#define OTA_STATE_APP_OK      (0x00000004UL) /* App 自检通过（回滚预留） */

/* ================================================================
 * 头部结构
 * ================================================================ */

typedef struct
{
    uint32_t magic;      /* OTA_MAGIC，不符即整区无效 */
    uint32_t image_size; /* 镜像字节数（≤ OTA_IMAGE_MAX） */
    uint32_t image_crc;  /* 镜像数据 CRC-32（ISO-HDLC） */
    uint32_t version;    /* 固件版本号（App 语义自定） */
    uint32_t state;      /* 镜像状态机 */
    uint32_t boot_count; /* bootloader 启动尝试计数（回滚预留） */
    uint32_t reserved[2];
} ota_header_t;

#ifdef __cplusplus
}
#endif

#endif /* __OTA_IMAGE_H */
