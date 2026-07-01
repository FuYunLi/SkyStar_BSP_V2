#define LOG_TAG "APP_POWER"

/**
 * @file app_power_demo.c
 * @brief 功率与电量监测自检 Demo 实现
 * @note 导出 power_read Shell 调试指令
 */

#include "app_power_demo.h"
#include "bsp_power.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief  初始化功率监测自检 Demo 并注册 Shell 指令
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_power_demo_init(void)
{
    log_i("Power Monitor Demo loaded.");
    return BSP_OK;
}

/**
 * @brief  单次读取功率及电量并打印
 */
static void s_power_read_once(void)
{
    float voltage = 0.0f;
    float current = 0.0f;
    float power   = 0.0f;
    bsp_status_t status = bsp_power_read(&voltage, &current, &power);
    if (status == BSP_OK)
    {
        /* 输出标准单位与毫级单位，方便调试和参数比对 */
        log_i("INA226 Power Data: Voltage = %.3f V, Current = %.3f A (%.1f mA), Power = %.3f W (%.1f mW)",
              voltage, current, current * 1000.0f, power, power * 1000.0f);
    }
    else
    {
        log_e("Failed to read INA226 power data! ret = %d", status);
    }
}

/**
 * @brief  Shell 命令入口
 */
static int shell_power_read(int argc, char *argv[])
{
    if (argc == 1)
    {
        s_power_read_once();
    }
    else if (argc == 3 && strcmp(argv[1], "-c") == 0)
    {
        int count = atoi(argv[2]);
        if (count <= 0)
        {
            log_e("Invalid count! Must be greater than 0.");
            return -1;
        }
        if (count > 100)
        {
            log_w("Count %d is too large, cap to 100.", count);
            count = 100;
        }

        log_i("Start reading power data continuously for %d times...", count);
        for (int i = 0; i < count; i++)
        {
            s_power_read_once();
            if (i < count - 1)
            {
                HAL_Delay(1000);
            }
        }
        log_i("Continuous reading done.");
    }
    else
    {
        log_i("Usage:");
        log_i("  power_read             Read voltage, current and power once");
        log_i("  power_read -c <count>  Read voltage, current and power continuously (max 100)");
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, power_read, shell_power_read, Read power voltage current and wattage);
