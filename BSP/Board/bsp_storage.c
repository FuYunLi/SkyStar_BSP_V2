/**
 * @file bsp_storage.c
 * @brief 板级通用存储服务实现源文件
 * @note 内部桥接并调用 LibDriver AT24Cxx 基础层例程
 */

#include "bsp_storage.h"
#include "driver_at24cxx_basic.h"

/**
 * @brief 初始化板级存储服务
 * @retval bsp_status_t 执行结果，成功返回 BSP_OK
 *
 * @note 本函数在系统启动时被调用以初始化板载 EEPROM 芯片
 * @example
 * bsp_status_t status = bsp_storage_init();
 * if (status == BSP_OK)
 * {
 *     // 初始化成功
 * }
 */
bsp_status_t bsp_storage_init(void)
{
    uint8_t res = at24cxx_basic_init(AT24C02, AT24CXX_ADDRESS_A000);
    if (res != 0)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}

/**
 * @brief 从存储设备读取数据
 * @param[in] address 存储起始物理地址 (0 - 255)
 * @param[out] buf 接收数据缓冲区指针
 * @param[in] len 待读取数据字节长度
 * @retval bsp_status_t 执行结果，成功返回 BSP_OK
 *
 * @note 读取范围不能越界 (最大 256 字节)
 * @example
 * uint8_t read_buf[16];
 * bsp_status_t status = bsp_storage_read(0x10, read_buf, sizeof(read_buf));
 * if (status == BSP_OK)
 * {
 *     // 读取数据成功
 * }
 */
bsp_status_t bsp_storage_read(uint32_t address, uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    if (address + len > 256)
    {
        return BSP_EINVAL;
    }

    uint8_t res = at24cxx_basic_read(address, buf, (uint16_t)len);
    if (res != 0)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}

/**
 * @brief 向存储设备写入数据
 * @param[in] address 存储起始物理地址 (0 - 255)
 * @param[in] buf 待写入数据缓存指针
 * @param[in] len 待写入数据字节长度
 * @retval bsp_status_t 执行结果，成功返回 BSP_OK
 *
 * @note 写入范围不能越界 (最大 256 字节)
 * @example
 * const uint8_t write_data[] = "Hello";
 * bsp_status_t status = bsp_storage_write(0x10, write_data, sizeof(write_data));
 * if (status == BSP_OK)
 * {
 *     // 写入数据成功
 * }
 */
bsp_status_t bsp_storage_write(uint32_t address, const uint8_t *buf, uint32_t len)
{
    if (buf == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    if (address + len > 256)
    {
        return BSP_EINVAL;
    }

    uint8_t res = at24cxx_basic_write(address, (uint8_t *)buf, (uint16_t)len);
    if (res != 0)
    {
        return BSP_ERROR;
    }
    return BSP_OK;
}
