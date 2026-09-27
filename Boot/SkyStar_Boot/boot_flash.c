/**
 * @file boot_flash.c
 * @brief Bootloader 侧片上 Flash 操作实现
 * @note F407 编程位宽按电压取 PSIZE=x32（3.3V）；解锁钥匙
 *       0x45670123/0xCDEF89AB，错误标志为 rc_w1 写 1 清零。
 *       bootloader 自身扇区（0-1）的擦除请求直接拒绝。
 */

#include "boot_flash.h"
#include "boot_hw.h"
#include "stm32f407xx.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define FLASH_KEY1     (0x45670123UL)
#define FLASH_KEY2     (0xCDEF89ABUL)
#define FLASH_PSIZE_X8  (0U << 8)
#define FLASH_PSIZE_X16 (1U << 8)
#define FLASH_PSIZE_X32 (2U << 8)

/* 变长扇区基址表（F407VET6 512KB） */
static const uint32_t s_sector_base[8] =
{
    0x08000000UL, 0x08004000UL, 0x08008000UL, 0x0800C000UL,
    0x08010000UL, 0x08020000UL, 0x08040000UL, 0x08060000UL
};

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 地址转扇区号
 */
uint8_t boot_flash_addr_to_sector(uint32_t addr)
{
    for (int8_t i = 7; i >= 0; i--)
    {
        if (addr >= s_sector_base[i])
        {
            return (uint8_t)i;
        }
    }

    return 0xFFU;
}

/**
 * @brief 解锁 Flash 控制寄存器
 */
static void s_flash_unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
    {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
}

/**
 * @brief 清除历史错误标志（rc_w1 写 1 清零）
 */
static void s_flash_clear_errors(void)
{
    /* EOP/OPERR/WRPERR/PGAERR/SOP 等错误位集中写 1 清除 */
    FLASH->SR = 0x000000F3UL;
}

/**
 * @brief 等待 BSY 清零并汇报错误
 */
static bool s_flash_wait_done(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U)
    {
    }

    if ((FLASH->SR & 0x0EU) != 0U) /* OPERR/WRPERR/PGAERR */
    {
        boot_hw_print("flash error SR=");
        boot_hw_print_hex(FLASH->SR);
        boot_hw_print("\r\n");
        s_flash_clear_errors();
        return false;
    }

    return true;
}

/**
 * @brief 扇区擦除（App 区边界受保护）
 */
void boot_flash_erase_sector(uint8_t sector)
{
    if (sector < BOOT_APP_SECTOR)
    {
        /* bootloader 自身扇区（0-1），拒绝自擦 */
        boot_hw_print("refuse erase boot sector ");
        boot_hw_print_hex(sector);
        boot_hw_print("\r\n");
        return;
    }

    s_flash_unlock();
    s_flash_clear_errors();

    /* SER + 扇区号 + 位宽 + STRT 一口气写入（同次写才生效） */
    FLASH->CR = FLASH_PSIZE_X32 | FLASH_CR_SER | ((uint32_t)sector << 3U);
    FLASH->CR |= FLASH_CR_STRT;

    (void)s_flash_wait_done();

    /* 清 SER，防误触发后续编程 */
    FLASH->CR &= ~FLASH_CR_SER;
}

/**
 * @brief 按字编程
 */
void boot_flash_write_words(uint32_t addr, const uint32_t *data, uint32_t word_count)
{
    if (addr < BOOT_APP_BASE)
    {
        boot_hw_print("refuse program below app base!\r\n");
        return;
    }

    s_flash_unlock();
    s_flash_clear_errors();

    FLASH->CR = FLASH_PSIZE_X32 | FLASH_CR_PG;

    for (uint32_t i = 0U; i < word_count; i++)
    {
        *(volatile uint32_t *)addr = data[i];

        if (!s_flash_wait_done())
        {
            break;
        }

        addr += 4U;
    }

    FLASH->CR &= ~FLASH_CR_PG;
}
