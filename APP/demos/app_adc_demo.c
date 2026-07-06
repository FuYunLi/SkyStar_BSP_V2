/**
 * @file app_adc_demo.c
 * @brief 内置 ADC 与电位器 Shell 自检演示模块实现源文件
 * @note 封装 Shell 接口，支持原始 LSB、电压以及滤波百分比开度的交互式实时输出。
 */

#define LOG_TAG "APP_ADC"

#include "app_adc_demo.h"
#include "port_adc.h"
#include "dev_potentiometer.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * 私有函数声明与实现
 * ================================================================ */

/**
 * @brief adc_read Shell 调试指令入口：读取电位器原始LSB值及电压（mV）
 */
static int shell_adc_read(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t raw_lsb = 0;
    uint32_t volt_mv = 0;

    bsp_status_t status = port_adc_read_raw(PORT_ADC_CH_POTENTIOMETER, &raw_lsb);
    if (status != BSP_OK)
    {
        log_e("Failed to read raw ADC! ret = %d", status);
        return -1;
    }

    status = port_adc_read_voltage(PORT_ADC_CH_POTENTIOMETER, &volt_mv);
    if (status != BSP_OK)
    {
        log_e("Failed to read voltage! ret = %d", status);
        return -1;
    }

    log_i("ADC RV1: LSB = %lu, Voltage = %lumV", raw_lsb, volt_mv);
    return 0;
}

/**
 * @brief pot_read Shell 调试指令入口：获取低通滤波后的电位器开度百分比
 */
static int shell_pot_read(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    float percent = 0.0f;
    uint32_t volt_mv = 0;

    /* 获取平滑后的电压 */
    bsp_status_t status = dev_potentiometer_get_voltage(&volt_mv);
    if (status != BSP_OK)
    {
        log_e("Failed to read potentiometer voltage! ret = %d", status);
        return -1;
    }

    /* 获取物理百分比 */
    status = dev_potentiometer_get_percent(&percent);
    if (status != BSP_OK)
    {
        log_e("Failed to read potentiometer percent! ret = %d", status);
        return -1;
    }

    log_i("Potentiometer RV1: Smooth Voltage = %lumV, Percent = %.1f%%", volt_mv, percent);
    return 0;
}

/**
 * @brief pot_monitor Shell 调试指令入口：以50ms间隔循环监测电位器，并在数值发生偏离时打印
 */
static int shell_pot_monitor(int argc, char *argv[])
{
    int check_count = 200; /* 默认检测 200 次（约 10 秒） */

    if (argc == 2)
    {
        check_count = atoi(argv[1]);
        if (check_count <= 0)
        {
            log_e("Invalid count! Must be greater than 0.");
            return -1;
        }
        if (check_count > 2000)
        {
            log_w("Count %d is too large, cap to 2000.", check_count);
            check_count = 2000;
        }
    }

    log_i("Start monitoring potentiometer (total %d checks, 50ms interval)...", check_count);

    float last_percent = -999.0f; /* 设为不合理初值强制首次打印 */

    for (int i = 0; i < check_count; i++)
    {
        float curr_percent = 0.0f;
        bsp_status_t status = dev_potentiometer_get_percent(&curr_percent);
        if (status == BSP_OK)
        {
            /* 电位器开度变化大于 0.5% 时打印，防止过于频繁且减少微小噪声干扰 */
            float diff = curr_percent - last_percent;
            if ((diff > 0.5f) || (diff < -0.5f))
            {
                log_i("Potentiometer RV1 Percent: %.1f%%", curr_percent);
                last_percent = curr_percent;
            }
        }
        else
        {
            log_e("Error reading potentiometer percent! ret = %d", status);
            break;
        }
        bsp_tick_delay_ms(50);
    }

    log_i("Potentiometer monitoring finished.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化内置 ADC 与电位器 Shell 自检演示模块
 */
bsp_status_t app_adc_demo_init(void)
{
    log_i("ADC Potentiometer Shell Demo loaded.");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, adc_read, shell_adc_read, Read raw ADC LSB and mV voltage once);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, pot_read, shell_pot_read, Read smoothed potentiometer percent once);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, pot_monitor, shell_pot_monitor, Monitor potentiometer percent interactively);
