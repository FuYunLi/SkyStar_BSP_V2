/**
 * @file boot_w25q.h
 * @brief Bootloader 侧 W25Q128 SPI Flash 裸驱动头文件
 * @note SPI2 轮询（无 DMA 无仲裁），片选 PE4 手动控制。
 *       指令集：JEDEC ID / 读 / 写使能 / 扇区擦 / 页编程 / 读状态。
 */

#ifndef __BOOT_W25Q_H
#define __BOOT_W25Q_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q_PAGE_SIZE  (256U)   /* 页编程大小 */
#define W25Q_SECTOR_SZ  (4096U)  /* 扇区擦除大小 */

/**
 * @brief 初始化 SPI2 与片选（21MHz，模式 0）
 * @retval true JEDEC ID 匹配 W25Q 家族
 */
bool boot_w25q_init(void);

/**
 * @brief 读取数据（0x03 慢读，SPI 时钟内安全）
 */
void boot_w25q_read(uint32_t addr, uint8_t *buf, uint32_t len);

/**
 * @brief 写入数据（自动按 256B 页对齐拆分，跨页安全）
 * @note 调用方保证目标区间已擦除（W25Q 只能 1→0）
 */
void boot_w25q_write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
 * @brief 擦除 4KB 扇区
 */
void boot_w25q_erase_sector(uint32_t addr);

/**
 * @brief 等待内部写操作完成（BUSY 位，带超时）
 */
void boot_w25q_wait_busy(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_W25Q_H */
