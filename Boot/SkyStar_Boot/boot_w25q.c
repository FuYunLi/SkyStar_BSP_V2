/**
 * @file boot_w25q.c
 * @brief Bootloader 侧 W25Q128 SPI Flash 裸驱动实现
 * @note 引脚：PB10=SCK、PC2=MISO、PC3=MOSI（AF5），片选 PE4。
 *       与主工程 SPI2 共用引脚——bootloader 独立运行时总线上
 *       只有它一个主机，无需仲裁。
 */

#include "boot_w25q.h"
#include "boot_hw.h"
#include "stm32f407xx.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define W25Q_CMD_JEDEC_ID  (0x9FU)
#define W25Q_CMD_READ      (0x03U)
#define W25Q_CMD_WRITE_EN  (0x06U)
#define W25Q_CMD_READ_SR1  (0x05U)
#define W25Q_CMD_SEC_ERASE (0x20U)
#define W25Q_CMD_PAGE_PROG (0x02U)
#define W25Q_JEDEC_W25Q    (0xEFU) /* 华邦厂商 ID */
#define W25Q_CS_MASK       (1UL << 4U)  /* PE4 */
#define W25Q_CS_HIGH()     (GPIOE->BSRR = W25Q_CS_MASK)
#define W25Q_CS_LOW()      (GPIOE->BSRR = W25Q_CS_MASK << 16U)
#define W25Q_BUSY_TIMEOUT  (100U)   /* ms，扇区擦最长约 400ms，按批次等待 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief SPI2 收发一个字节（全双工轮询）
 */
static uint8_t s_w25q_spi_byte(uint8_t tx)
{
    while ((SPI2->SR & SPI_SR_TXE) == 0U)
    {
    }
    SPI2->DR = tx;

    while ((SPI2->SR & SPI_SR_RXNE) == 0U)
    {
    }

    return (uint8_t)SPI2->DR;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 SPI2 与片选
 */
bool boot_w25q_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOEEN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    /* PB10=SCK、PC2=MISO、PC3=MOSI（AF5）复用推挽 */
    GPIOB->MODER &= ~(3UL << 20);
    GPIOB->MODER |= (2UL << 20);
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~(0xFUL << 8)) | (5UL << 8);

    GPIOC->MODER &= ~((3UL << 4) | (3UL << 6));
    GPIOC->MODER |= (2UL << 4) | (2UL << 6);
    GPIOC->AFR[0] = (GPIOC->AFR[0] & ~((0xFUL << 8) | (0xFUL << 12))) |
                    (5UL << 8) | (5UL << 12);

    /* PE4 片选推挽输出，默认高（未选中） */
    GPIOE->MODER &= ~(3UL << 8);
    GPIOE->MODER |= (1UL << 8);
    W25Q_CS_HIGH();

    /* 主机模式、模式 0、8 位、软件 NSS、fPCLK/2 = 21MHz */
    SPI2->CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_MSTR | SPI_CR1_SPE;

    /* JEDEC ID 验活：首字节应为华邦 0xEF */
    uint8_t id0, id1, id2;
    W25Q_CS_LOW();
    (void)s_w25q_spi_byte(W25Q_CMD_JEDEC_ID);
    id0 = s_w25q_spi_byte(0xFFU);
    id1 = s_w25q_spi_byte(0xFFU);
    id2 = s_w25q_spi_byte(0xFFU);
    W25Q_CS_HIGH();

    boot_hw_print("W25Q JEDEC: ");
    boot_hw_print_hex(((uint32_t)id0 << 16) | ((uint32_t)id1 << 8) | id2);
    boot_hw_print("\r\n");

    return (id0 == W25Q_JEDEC_W25Q);
}

/**
 * @brief 等待内部写操作完成
 */
void boot_w25q_wait_busy(void)
{
    uint32_t start = boot_hw_tick_ms();

    for (;;)
    {
        uint8_t sr;

        W25Q_CS_LOW();
        (void)s_w25q_spi_byte(W25Q_CMD_READ_SR1);
        sr = s_w25q_spi_byte(0xFFU);
        W25Q_CS_HIGH();

        if ((sr & 0x01U) == 0U) /* BUSY 位清零 */
        {
            return;
        }

        if ((boot_hw_tick_ms() - start) > W25Q_BUSY_TIMEOUT)
        {
            boot_hw_print("W25Q busy timeout!\r\n");
            return;
        }
    }
}

/**
 * @brief 读取数据
 */
void boot_w25q_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    boot_w25q_wait_busy();

    W25Q_CS_LOW();
    (void)s_w25q_spi_byte(W25Q_CMD_READ);
    (void)s_w25q_spi_byte((uint8_t)(addr >> 16));
    (void)s_w25q_spi_byte((uint8_t)(addr >> 8));
    (void)s_w25q_spi_byte((uint8_t)addr);

    for (uint32_t i = 0U; i < len; i++)
    {
        buf[i] = s_w25q_spi_byte(0xFFU);
    }
    W25Q_CS_HIGH();
}

/**
 * @brief 写入数据（页对齐拆分，先写使能后页编程）
 */
void boot_w25q_write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    while (len > 0U)
    {
        /* 页内剩余空间决定本页可写长度 */
        uint32_t page_off = addr % W25Q_PAGE_SIZE;
        uint32_t chunk = W25Q_PAGE_SIZE - page_off;
        if (chunk > len)
        {
            chunk = len;
        }

        boot_w25q_wait_busy();

        W25Q_CS_LOW();
        (void)s_w25q_spi_byte(W25Q_CMD_WRITE_EN);
        W25Q_CS_HIGH();

        W25Q_CS_LOW();
        (void)s_w25q_spi_byte(W25Q_CMD_PAGE_PROG);
        (void)s_w25q_spi_byte((uint8_t)(addr >> 16));
        (void)s_w25q_spi_byte((uint8_t)(addr >> 8));
        (void)s_w25q_spi_byte((uint8_t)addr);
        for (uint32_t i = 0U; i < chunk; i++)
        {
            (void)s_w25q_spi_byte(buf[i]);
        }
        W25Q_CS_HIGH();

        boot_w25q_wait_busy();
        addr += chunk;
        buf += chunk;
        len -= chunk;
    }
}

/**
 * @brief 擦除 4KB 扇区
 */
void boot_w25q_erase_sector(uint32_t addr)
{
    boot_w25q_wait_busy();

    W25Q_CS_LOW();
    (void)s_w25q_spi_byte(W25Q_CMD_WRITE_EN);
    W25Q_CS_HIGH();

    /* 24 位地址以 4KB 对齐 */
    addr &= ~(W25Q_SECTOR_SZ - 1U);

    W25Q_CS_LOW();
    (void)s_w25q_spi_byte(W25Q_CMD_SEC_ERASE);
    (void)s_w25q_spi_byte((uint8_t)(addr >> 16));
    (void)s_w25q_spi_byte((uint8_t)(addr >> 8));
    (void)s_w25q_spi_byte((uint8_t)addr);
    W25Q_CS_HIGH();

    boot_w25q_wait_busy();
}
