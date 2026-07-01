#define LOG_TAG "APP_SENSOR"

/**
 * @file app_sensor_demo.c
 * @brief 温湿度传感器测试 Demo 实现
 * @note 导出 aht20_read Shell 调试指令
 */

#include "app_sensor_demo.h"
#include "bsp_sensor.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * @brief  初始化温湿度测试 Demo 并注册 Shell 指令
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_sensor_demo_init(void)
{
    /* 温湿度传感器已在板级总线统一初始化，此处仅供模块挂载演示 */
    log_i("Environmental Sensor Demo loaded.");
    return BSP_OK;
}

/**
 * @brief  单次读取温湿度并打印
 */
static void s_sensor_read_once(void)
{
    float temp = 0.0f;
    float hum  = 0.0f;
    bsp_status_t status = bsp_sensor_read_environmental(&temp, &hum);
    if (status == BSP_OK)
    {
        log_i("AHT20 Environmental Data: Temp = %.1f C, Hum = %.1f %%RH", temp, hum);
    }
    else
    {
        log_e("Failed to read AHT20 data! ret = %d", status);
    }
}

/**
 * @brief  Shell 命令入口
 */
static int shell_aht20_read(int argc, char *argv[])
{
    if (argc == 1)
    {
        s_sensor_read_once();
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

        log_i("Start reading environmental data continuously for %d times...", count);
        for (int i = 0; i < count; i++)
        {
            s_sensor_read_once();
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
        log_i("  aht20_read             Read temperature and humidity once");
        log_i("  aht20_read -c <count>  Read temperature and humidity continuously (max 100)");
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, aht20_read, shell_aht20_read, Read environmental temperature and humidity);
