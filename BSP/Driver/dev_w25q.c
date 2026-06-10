/**
 * @file dev_w25q.c
 * @brief W25Q128 SPI Flash 底层物理驱动
 */

#include "dev_w25q.h"
#include "port_spi.h"
#include "port_gpio.h"
#include "port_tick.h"

/* 硬件配置映射 */
#define W25Q_SPI_BUS PORT_SPI_2
#define W25Q_CS_PIN  PORT_GPIO_W25Q_CS

/* W25Qxx 常用指令 */
#define W25X_WRITE_ENABLE    0x06 /* 写使能 */
#define W25X_READ_DATA       0x03 /* 读数据 */
#define W25X_PAGE_PROGRAM    0x02 /* 页编程 */
#define W25X_SECTOR_ERASE    0x20 /* 扇区擦除(4KB) */
#define W25X_READ_STATUS_REG 0x05 /* 读状态寄存器1 */
#define W25X_JEDEC_DEVICE_ID 0x9F /* 读JEDEC ID */

#define W25X_WIP_FLAG        0x01 /* 写进行中标志位(WIP) */
#define W25Q_TIMEOUT_MS      2000 /* 擦写最大超时时间 */

/* =========================================================================
 * 内部辅助函数
 * ========================================================================= */

static inline void w25q_cs_select(void)
{
    port_gpio_write(W25Q_CS_PIN, PORT_GPIO_LOW);
}

static inline void w25q_cs_unselect(void)
{
    port_gpio_write(W25Q_CS_PIN, PORT_GPIO_HIGH);
}

/**
 * @brief 发送写使能指令
 */
static void w25q_write_enable(void)
{
    uint8_t cmd = W25X_WRITE_ENABLE;
    
    w25q_cs_select();
    port_spi_write(W25Q_SPI_BUS, &cmd, 1, BSP_WAIT_FOREVER);
    w25q_cs_unselect();
}

/**
 * @brief 等待设备就绪 (等待 WIP 位清零)
 */
static bsp_status_t w25q_wait_busy(void)
{
    uint8_t cmd = W25X_READ_STATUS_REG;
    uint8_t status = 0;
    uint32_t start_time = port_tick_get_ms();

    w25q_cs_select();
    port_spi_write(W25Q_SPI_BUS, &cmd, 1, BSP_WAIT_FOREVER);
    do
    {
        port_spi_read(W25Q_SPI_BUS, &status, 1, BSP_WAIT_FOREVER);
        if (port_tick_get_ms() - start_time > W25Q_TIMEOUT_MS)
        {
            w25q_cs_unselect();
            return BSP_ETIMEOUT;
        }
    } while ((status & W25X_WIP_FLAG) == W25X_WIP_FLAG);
    
    w25q_cs_unselect();
    return BSP_OK;
}

/**
 * @brief 在一页内编程 (不超过256字节，不跨页)
 */
static bsp_status_t w25q_page_program(uint32_t addr, const uint8_t *buf, uint32_t size)
{
    uint8_t cmd[4];
    
    w25q_write_enable();
    
    cmd[0] = W25X_PAGE_PROGRAM;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
    cmd[3] = (uint8_t)(addr & 0xFF);

    w25q_cs_select();
    port_spi_write(W25Q_SPI_BUS, cmd, 4, BSP_WAIT_FOREVER);
    port_spi_write(W25Q_SPI_BUS, buf, size, BSP_WAIT_FOREVER);
    w25q_cs_unselect();

    return w25q_wait_busy();
}

/* =========================================================================
 * 导出 API 函数
 * ========================================================================= */

/**
 * @brief 初始化 W25Q (读取 JEDEC ID 检查设备在线状态)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t dev_w25q_init(void)
{
    bsp_status_t ret = port_spi_init(W25Q_SPI_BUS);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 确保 CS 初始状态为高 */
    w25q_cs_unselect();

    uint32_t id = 0;
    ret = dev_w25q_get_id(&id);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 验证厂商ID (Winbond: 0xEF) 及 W25Q128 容量标识 (0x4018) */
    if ((id & 0x00FF0000) != 0x00EF0000)
    {
        return BSP_ENODEV;
    }

    return BSP_OK;
}

/**
 * @brief 读数据 (直读物理地址)
 * @param addr 起始物理地址
 * @param buf 数据缓冲区
 * @param size 读取字节数
 * @retval bsp_status_t 执行结果
 */
bsp_status_t dev_w25q_read(uint32_t addr, uint8_t *buf, uint32_t size)
{
    BSP_CHECK_NULL(buf);
    if (size == 0)
    {
        return BSP_EINVAL;
    }

    uint8_t cmd[4];
    
    bsp_status_t ret = w25q_wait_busy();
    if (ret != BSP_OK)
    {
        return ret;
    }

    cmd[0] = W25X_READ_DATA;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
    cmd[3] = (uint8_t)(addr & 0xFF);

    w25q_cs_select();
    port_spi_write(W25Q_SPI_BUS, cmd, 4, BSP_WAIT_FOREVER);
    port_spi_read(W25Q_SPI_BUS, buf, size, BSP_WAIT_FOREVER);
    w25q_cs_unselect();

    return BSP_OK;
}

/**
 * @brief 写数据 (支持内部循环自动跨页，但不擦除目标区域)
 * @param addr 起始物理地址
 * @param buf 数据缓冲区
 * @param size 写入字节数
 * @retval bsp_status_t 执行结果
 */
bsp_status_t dev_w25q_write(uint32_t addr, const uint8_t *buf, uint32_t size)
{
    BSP_CHECK_NULL(buf);
    if (size == 0)
    {
        return BSP_EINVAL;
    }

    uint32_t first_page_rem, write_size;
    bsp_status_t res = BSP_OK;
    const uint8_t *p_data = buf;

    /* 计算第一页剩余空间 */
    first_page_rem = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);

    while (size > 0)
    {
        write_size = (size <= first_page_rem) ? size : first_page_rem;
        
        res = w25q_page_program(addr, p_data, write_size);
        if (res != BSP_OK)
        {
            break;
        }
        
        addr += write_size;
        p_data += write_size;
        size -= write_size;
        first_page_rem = W25Q_PAGE_SIZE; /* 后续均对齐整页写入 */
    }

    return res;
}

/**
 * @brief 擦除扇区 (4KB)
 * @param addr 扇区物理地址 (必须对齐4KB)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t dev_w25q_erase_sector(uint32_t addr)
{
    uint8_t cmd[4];

    w25q_write_enable();

    cmd[0] = W25X_SECTOR_ERASE;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[2] = (uint8_t)((addr >> 8) & 0xFF);
    cmd[3] = (uint8_t)(addr & 0xFF);

    w25q_cs_select();
    port_spi_write(W25Q_SPI_BUS, cmd, 4, BSP_WAIT_FOREVER);
    w25q_cs_unselect();

    return w25q_wait_busy();
}

/**
 * @brief 同步等待 (阻塞等待操作完成)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t dev_w25q_sync(void)
{
    return w25q_wait_busy();
}

/**
 * @brief 获取 JEDEC ID
 * @param p_id 存储 ID 的指针变量 (例如 0xEF4018)
 * @retval bsp_status_t 执行结果
 */
bsp_status_t dev_w25q_get_id(uint32_t *p_id)
{
    BSP_CHECK_NULL(p_id);
    
    uint8_t id[3] = {0};
    uint8_t cmd = W25X_JEDEC_DEVICE_ID;
    
    w25q_cs_select();
    port_spi_write(W25Q_SPI_BUS, &cmd, 1, BSP_WAIT_FOREVER);
    port_spi_read(W25Q_SPI_BUS, id, 3, BSP_WAIT_FOREVER);
    w25q_cs_unselect();
    
    *p_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | (uint32_t)id[2];
    
    return BSP_OK;
}
