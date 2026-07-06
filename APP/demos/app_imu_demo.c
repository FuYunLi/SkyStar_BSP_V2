/**
 * @file app_imu_demo.c
 * @brief 自检演示模块——板载姿态传感器 Shell 自检指令实现
 * @note 导出 imu_read(原始值) 及 imu_attitude(姿态角度值) Shell 控制命令。
 */

#define LOG_TAG "APP_IMU"

#include "app_imu_demo.h"
#include "bsp_imu.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================================================
 * 私有函数声明与实现
 * ================================================================ */

/**
 * @brief 读取单次 IMU 物理原始读数并打印回显
 */
static void s_imu_read_once(void)
{
    bsp_imu_raw_t raw = {0};
    bsp_status_t status = bsp_imu_get_raw(&raw);
    if (status == BSP_OK)
    {
        log_i("IMU Raw: Accel(%.3f, %.3f, %.3f) g, Gyro(%.2f, %.2f, %.2f) dps, Temp = %.2f C",
              raw.accel_x, raw.accel_y, raw.accel_z,
              raw.gyro_x, raw.gyro_y, raw.gyro_z,
              raw.temp);
    }
    else
    {
        log_e("Failed to read IMU raw data! ret = %d", status);
    }
}

/**
 * @brief 读取单次 IMU 解算后的姿态角并打印回显
 */
static void s_imu_attitude_once(void)
{
    bsp_imu_attitude_t att = {0};
    bsp_status_t status = bsp_imu_get_attitude(&att);
    if (status == BSP_OK)
    {
        log_i("IMU Attitude: Pitch = %.2f deg, Roll = %.2f deg", att.pitch, att.roll);
    }
    else
    {
        log_e("Failed to read IMU attitude! ret = %d", status);
    }
}

/**
 * @brief imu_read Shell 快捷指令入口函数
 * @param argc 参数个数
 * @param argv 参数列表指针数组
 * @return int 执行状态码
 */
static int shell_imu_read(int argc, char *argv[])
{
    if (argc == 1)
    {
        s_imu_read_once();
    }
    else if (argc == 3 && strcmp(argv[1], "-c") == 0)
    {
        int count = atoi(argv[2]);
        if (count <= 0)
        {
            log_e("Invalid count! Must be greater than 0.");
            return -1;
        }
        if (count > 200)
        {
            log_w("Count %d is too large, cap to 200.", count);
            count = 200;
        }

        log_i("Start reading IMU raw data continuously for %d times...", count);
        for (int i = 0; i < count; i++)
        {
            s_imu_read_once();
            if (i < count - 1)
            {
                bsp_tick_delay_ms(100);
            }
        }
        log_i("Continuous reading done.");
    }
    else
    {
        log_i("Usage:");
        log_i("  imu_read             Read accelerometer and gyroscope raw data once");
        log_i("  imu_read -c <count>  Read raw data continuously (max 200, 100ms interval)");
    }
    return 0;
}

/**
 * @brief imu_attitude Shell 快捷指令入口函数
 * @param argc 参数个数
 * @param argv 参数列表指针数组
 * @return int 执行状态码
 */
static int shell_imu_attitude(int argc, char *argv[])
{
    if (argc == 1)
    {
        s_imu_attitude_once();
    }
    else if (argc == 3 && strcmp(argv[1], "-c") == 0)
    {
        int count = atoi(argv[2]);
        if (count <= 0)
        {
            log_e("Invalid count! Must be greater than 0.");
            return -1;
        }
        if (count > 200)
        {
            log_w("Count %d is too large, cap to 200.", count);
            count = 200;
        }

        log_i("Start reading IMU attitude continuously for %d times...", count);
        for (int i = 0; i < count; i++)
        {
            s_imu_attitude_once();
            if (i < count - 1)
            {
                bsp_tick_delay_ms(100);
            }
        }
        log_i("Continuous attitude calculation done.");
    }
    else
    {
        log_i("Usage:");
        log_i("  imu_attitude             Read pitch and roll once");
        log_i("  imu_attitude -c <count>  Read attitude continuously (max 200, 100ms interval)");
    }
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化姿态传感器自检演示模块
 */
bsp_status_t app_imu_demo_init(void)
{
    log_i("IMU Shell Demo loaded.");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, imu_read, shell_imu_read, Read IMU 6-axis raw data);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, imu_attitude, shell_imu_attitude, Read IMU pitch and roll attitude);

