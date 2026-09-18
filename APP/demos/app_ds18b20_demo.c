/**
 * @file app_ds18b20_demo.c
 * @brief DS18B20 温度采集自检演示实现
 * @note 【未验证】外部传感器尚未接线实测。转换等待为固定 750ms
 *       （12 位最长转换时间），阻塞于 Shell 上下文可接受。
 */

#define LOG_TAG "APP_DS18B20"

#include "app_ds18b20_demo.h"
#include "bsp_logger.h"
#include "dev_ds18b20.h"
#include "shell.h"

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief ds18b20 Shell 指令入口：读取温度
 */
static int shell_ds18b20(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int16_t temp_deci = 0;
    bsp_status_t ret = dev_ds18b20_read_temp(&temp_deci);

    if (ret == BSP_ENODEV)
    {
        log_e("no sensor on one-wire bus (check wiring & 4.7k pullup).");
        return -1;
    }
    if (ret == BSP_ERROR)
    {
        log_e("CRC check failed!");
        return -1;
    }
    if (ret != BSP_OK)
    {
        log_e("read failed! ret = %d", ret);
        return -1;
    }

    /* 拆分整数/小数位打印，规避浮点格式化 */
    log_i("DS18B20 Temperature = %d.%d C", (int)(temp_deci / 10), (int)(temp_deci % 10));
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 DS18B20 演示模块
 */
bsp_status_t app_ds18b20_demo_init(void)
{
    bsp_status_t ret = dev_ds18b20_init();
    if (ret != BSP_OK)
    {
        return ret;
    }

    log_i("DS18B20 Demo loaded. Try: ds18b20");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ds18b20, shell_ds18b20, Read DS18B20 temperature (0.1C));
