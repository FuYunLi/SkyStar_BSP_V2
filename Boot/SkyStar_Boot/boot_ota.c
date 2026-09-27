/**
 * @file boot_ota.c
 * @brief Bootloader OTA 流水线实现
 * @note 流程：头部三验（magic/state/CRC）→ 擦 App 扇区 2-7 → 4KB 块
 *       拷贝 → 片上 CRC 复验 → 头部置 JUMPED → 跳转。
 *       断电安全性：READY 之前镜像无效被忽略；拷贝幂等可重入；
 *       跳转前完成外设去初始化三件套（SysTick 停摆/中断关闭/VTOR 重指）。
 */

#include "boot_ota.h"
#include "ota_image.h"
#include "boot_flash.h"
#include "boot_hw.h"
#include "boot_w25q.h"
#include "stm32f407xx.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define COPY_CHUNK_SIZE (4096U) /* 拷贝块（与 W25Q 扇区对齐） */
#define CRC32_INIT      (0xFFFFFFFFUL)
#define CRC32_FINAL_XOR (0xFFFFFFFFUL)
#define CRC32_POLY      (0xEDB88320UL)

/* ================================================================
 * CRC-32（逐位法，参数与主工程 app_crc_demo 一致）
 * ================================================================ */

uint32_t boot_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = CRC32_INIT;

    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)data[i];

        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 1UL) != 0UL) ? ((crc >> 1) ^ CRC32_POLY) : (crc >> 1);
        }
    }

    return crc ^ CRC32_FINAL_XOR;
}

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 重写 OTA 头部（头部独占扇区：擦后重编程）
 */
static void s_ota_write_header(const ota_header_t *hdr)
{
    boot_w25q_erase_sector(OTA_HEADER_ADDR);
    boot_w25q_write(OTA_HEADER_ADDR, (const uint8_t *)hdr, sizeof(ota_header_t));
}

/**
 * @brief 校验镜像 CRC（分块从 W25Q 读取，流式累加，内存零拷贝）
 */
static bool s_ota_verify_image_crc(uint32_t size, uint32_t expect_crc)
{
    static uint8_t chunk[COPY_CHUNK_SIZE]; /* 4KB 静态缓冲，bootloader 独占 RAM */
    uint32_t crc = CRC32_INIT;
    uint32_t addr = OTA_IMAGE_ADDR;
    uint32_t remaining = size;

    while (remaining > 0U)
    {
        uint32_t n = (remaining > COPY_CHUNK_SIZE) ? COPY_CHUNK_SIZE : remaining;
        boot_w25q_read(addr, chunk, n);

        for (uint32_t i = 0U; i < n; i++)
        {
            crc ^= (uint32_t)chunk[i];
            for (uint32_t bit = 0U; bit < 8U; bit++)
            {
                crc = ((crc & 1UL) != 0UL) ? ((crc >> 1) ^ CRC32_POLY) : (crc >> 1);
            }
        }

        addr += n;
        remaining -= n;
    }

    return (crc ^ CRC32_FINAL_XOR) == expect_crc;
}

/**
 * @brief 擦除 App 区覆盖镜像所需的全部扇区（2-7）
 */
static void s_ota_erase_app(void)
{
    for (uint8_t sector = BOOT_APP_SECTOR; sector <= 7U; sector++)
    {
        boot_hw_print("erase sector ");
        boot_hw_print_hex(sector);
        boot_hw_print("...\r\n");
        boot_flash_erase_sector(sector);
    }
}

/**
 * @brief 从 W25Q 拷贝镜像到片上 Flash（4KB 块 + 字编程 + 尾部补齐）
 */
static bool s_ota_copy_image(uint32_t size)
{
    static uint32_t chunk[COPY_CHUNK_SIZE / 4U]; /* 4KB 字缓冲 */

    for (uint32_t off = 0U; off < size; off += COPY_CHUNK_SIZE)
    {
        uint32_t n = size - off;
        if (n > COPY_CHUNK_SIZE)
        {
            n = COPY_CHUNK_SIZE;
        }

        boot_w25q_read(OTA_IMAGE_ADDR + off, (uint8_t *)chunk, n);

        /* 尾部不足一字时以 0xFF 补齐（等价于未编程位） */
        uint32_t word_count = (n + 3U) / 4U;
        for (uint32_t i = n; i < word_count * 4U; i++)
        {
            ((uint8_t *)chunk)[i] = 0xFFU;
        }

        boot_flash_write_words(BOOT_APP_BASE + off, chunk, word_count);

        boot_hw_print("copy ");
        boot_hw_print_hex(off);
        boot_hw_print("/\r\n");
    }

    return true;
}

/**
 * @brief 片上 CRC 复验（直接读内存）
 */
static bool s_ota_verify_flash(uint32_t size, uint32_t expect_crc)
{
    uint32_t crc = CRC32_INIT;
    const uint8_t *p = (const uint8_t *)BOOT_APP_BASE;

    for (uint32_t i = 0U; i < size; i++)
    {
        crc ^= (uint32_t)p[i];
        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 1UL) != 0UL) ? ((crc >> 1) ^ CRC32_POLY) : (crc >> 1);
        }
    }

    return (crc ^ CRC32_FINAL_XOR) == expect_crc;
}

/**
 * @brief 跳转 App（三件套：关中断停 SysTick → VTOR/MSP 重设 → 函数指针跳转）
 */
static void s_ota_jump_to_app(void)
{
    uint32_t app_sp = *(volatile uint32_t *)BOOT_APP_BASE;
    uint32_t app_pc = *(volatile uint32_t *)(BOOT_APP_BASE + 4U);

    /* 栈指针必须落在 SRAM，复位向量必须落在 App 区内 */
    if (((app_sp & 0xFF000000UL) != 0x20000000UL) ||
        (app_pc < BOOT_APP_BASE) || (app_pc >= BOOT_APP_END))
    {
        boot_hw_print("app invalid! sp=");
        boot_hw_print_hex(app_sp);
        boot_hw_print(" pc=");
        boot_hw_print_hex(app_pc);
        boot_hw_print("\r\n");
        return;
    }

    /* 外设去初始化：停时基、关串口与 SPI（时钟门控保留，App 会重配） */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    USART1->CR1 = 0U;
    SPI2->CR1 = 0U;

    __disable_irq();

    SCB->VTOR = BOOT_APP_BASE;
    __DSB();
    __ISB();

    __set_MSP(app_sp);
    ((void (*)(void))app_pc)();
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief OTA 主流程
 */
void boot_ota_process(void)
{
    ota_header_t hdr;

    boot_w25q_read(OTA_HEADER_ADDR, (uint8_t *)&hdr, sizeof(hdr));

    if (hdr.magic != OTA_MAGIC)
    {
        boot_hw_print("no ota image (magic), jumping app.\r\n");
        s_ota_jump_to_app();
        return;
    }

    if (hdr.state != OTA_STATE_READY)
    {
        boot_hw_print("ota state not READY (");
        boot_hw_print_hex(hdr.state);
        boot_hw_print("), jumping app.\r\n");
        s_ota_jump_to_app();
        return;
    }

    if ((hdr.image_size == 0U) || (hdr.image_size > OTA_IMAGE_MAX))
    {
        boot_hw_print("invalid image size, jumping app.\r\n");
        s_ota_jump_to_app();
        return;
    }

    boot_hw_print("ota image v");
    boot_hw_print_hex(hdr.version);
    boot_hw_print(" size=");
    boot_hw_print_hex(hdr.image_size);
    boot_hw_print(", verifying...\r\n");

    /* 1. W25Q 侧 CRC 校验 */
    if (!s_ota_verify_image_crc(hdr.image_size, hdr.image_crc))
    {
        boot_hw_print("image crc FAIL, jumping app.\r\n");
        s_ota_jump_to_app();
        return;
    }

    /* 2. 擦 App 区扇区 2-7 */
    s_ota_erase_app();

    /* 3. 拷贝 */
    boot_hw_print("copying...\r\n");
    (void)s_ota_copy_image(hdr.image_size);

    /* 4. 片上复验 */
    if (!s_ota_verify_flash(hdr.image_size, hdr.image_crc))
    {
        boot_hw_print("flash crc FAIL! retry by reboot.\r\n");
        /* 保持 READY：重启后 bootloader 重擦重拷（幂等） */
        return;
    }

    /* 5. 标记已跳转并进入 App */
    hdr.state = OTA_STATE_JUMPED;
    s_ota_write_header(&hdr);

    boot_hw_print("booting app...\r\n");
    s_ota_jump_to_app();
}
