/**
 * @file app_storage_demo.c
 * @brief 板载存储服务演示与测试模块源文件
 * @note 导出存储测试命令到 Letter Shell，并在系统初始化时打印初始化状态。
 */

#include "app_storage_demo.h"
#include "bsp_storage.h"
#include "shell.h"
#define LOG_TAG "STORAGE_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief 执行存储设备读写自检测试
 * @note 往地址 0x10 写入并回读测试数据，校验数据一致性
 */
void shell_storage_test(void)
{
    const uint8_t test_addr = 0x10;
    const char write_data[] = "SkyStar Storage Test!";
    char read_buf[sizeof(write_data)];
    bsp_status_t status;

    log_i("--- AT24C02 EEPROM Read Write Test ---");
    log_i("Address: 0x%02X, Length: %d bytes", test_addr, (int)sizeof(write_data));
    log_i("Write payload: \"%s\"", write_data);

    /* 1. 写入测试数据 */
    status = bsp_storage_write(test_addr, (const uint8_t *)write_data, sizeof(write_data));
    if (status != BSP_OK)
    {
        log_e("bsp_storage_write failed, status: %d", (int)status);
        return;
    }
    log_i("bsp_storage_write success");

    /* 2. 延迟回读 (EEPROM 物理写周期通常在 5ms 左右，LibDriver 内部已包含物理延迟) */
    memset(read_buf, 0, sizeof(read_buf));
    status = bsp_storage_read(test_addr, (uint8_t *)read_buf, sizeof(read_buf));
    if (status != BSP_OK)
    {
        log_e("bsp_storage_read failed, status: %d", (int)status);
        return;
    }
    log_i("bsp_storage_read success, payload: \"%s\"", read_buf);

    /* 3. 数据一致性比对 */
    if (memcmp(write_data, read_buf, sizeof(write_data)) == 0)
    {
        log_i("Verification SUCCESS: data matches perfectly!");
    }
    else
    {
        log_e("Verification FAILED: data mismatch!");
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, storage_test, shell_storage_test, "AT24C02 read/write verification test");

/**
 * @brief 初始化板载存储服务演示模块
 * @retval bsp_status_t 执行结果，成功返回 BSP_OK
 */
bsp_status_t app_storage_demo_init(void)
{
    log_i("[APP] Storage demo module initialized successfully");
    return BSP_OK;
}
