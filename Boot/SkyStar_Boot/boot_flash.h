/**
 * @file boot_flash.h
 * @brief Bootloader 侧片上 Flash 操作头文件
 * @note F407 变长扇区：扇区 0-3=16KB、4=64KB、5-7=128KB。
 *       本模块只允许操作 App 区扇区 2-7，bootloader 自身扇区
 *       （0-1）受 boot_flash_erase() 的边界检查保护，永不自擦。
 */

#ifndef __BOOT_FLASH_H
#define __BOOT_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_APP_BASE   (0x08008000UL) /* App 基址 */
#define BOOT_APP_END    (0x08080000UL) /* App 区结束（不含） */
#define BOOT_APP_SECTOR (2U)           /* App 区起始扇区号 */

/**
 * @brief 片上 Flash 地址转扇区号
 * @retval 0xFF 地址越界
 */
uint8_t boot_flash_addr_to_sector(uint32_t addr);

/**
 * @brief 扇区擦除（阻塞，128KB 扇区最长约 2s）
 */
void boot_flash_erase_sector(uint8_t sector);

/**
 * @brief 按字（32 位）编程一段数据
 * @note 调用方保证目标区域已擦除；逐字等待 BSY，运行于 Flash
 *       时总线停等属正常现象
 */
void boot_flash_write_words(uint32_t addr, const uint32_t *data, uint32_t word_count);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_FLASH_H */
