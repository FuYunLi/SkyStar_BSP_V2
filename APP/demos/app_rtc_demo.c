#define LOG_TAG "APP_RTC"

/**
 * @file app_rtc_demo.c
 * @brief SD3078 RTC 演示与自检模块实现
 * @note 遵循 Allman 风格及 BSP 架构规范
 */

#include "app_rtc_demo.h"
#include "dev_sd3078.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>

/* 星期名字映射 */
static const char *const WEEKDAY_NAMES[] = 
{
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

/**
 * @brief 基姆拉尔森星期计算公式
 * 
 * @param year 完整年份 (如 2026)
 * @param month 月份 (1~12)
 * @param day 日期 (1~31)
 * @return uint8_t 0 = Sunday, 1 = Monday, ..., 6 = Saturday
 */
static uint8_t get_weekday_by_date(uint16_t year, uint8_t month, uint8_t day)
{
    if (month == 1 || month == 2)
    {
        month += 12;
        year--;
    }
    int w = (day + 2 * month + 3 * (month + 1) / 5 + year + year / 4 - year / 100 + year / 400 + 1) % 7;
    return (uint8_t)w;
}

/**
 * @brief SD3078 外部中断回调函数
 */
static void app_rtc_demo_int_callback(void)
{
    log_i("SD3078 EXTI3 Interrupt Triggered!");
    /* 物理清场，释放 INT 引脚高电平，保证后续能够再次触发中断 */
    (void)dev_sd3078_clear_alarm_flag();
    (void)dev_sd3078_clear_countdown_flag();
}

/**
 * @brief 初始化 RTC 演示模块
 */
bsp_status_t app_rtc_demo_init(void)
{
    bsp_status_t status = BSP_OK;

    /* 1. 初始化 SD3078 驱动 (含关闭充电及清除遗留中断) */
    status = dev_sd3078_init();
    if (status != BSP_OK)
    {
        log_e("Failed to initialize SD3078 driver! ret = %d", status);
        return status;
    }

    /* 2. 挂载外部中断回调，以监测定时器或报警引脚信号 */
    status = dev_sd3078_int_attach(app_rtc_demo_int_callback);
    if (status != BSP_OK)
    {
        log_w("Failed to attach EXTI3 callback (JUMP5 jumper may not be connected), ret = %d", status);
    }

    log_i("SD3078 RTC Demo Initialized Successfully.");
    return BSP_OK;
}

/* =========================================================================
 * Shell 指令导出
 * ========================================================================= */

/**
 * @brief 测试指令：读取当前时间、温度和电池电量状态
 */
static void shell_rtc_get(void)
{
    sd3078_time_t rtc_time = {0};
    int8_t        temp     = 0;
    bool          bat_low  = false;
    bsp_status_t  status   = BSP_OK;

    /* 1. 获取时间 */
    status = dev_sd3078_get_time(&rtc_time);
    if (status != BSP_OK)
    {
        log_e("Failed to get RTC time! ret = %d", status);
        return;
    }

    /* 2. 获取温度 */
    (void)dev_sd3078_get_temperature(&temp);

    /* 3. 获取电池欠压状态 */
    (void)dev_sd3078_get_battery_status(&bat_low);

    /* 4. 格式化输出 */
    const char *weekday_str = (rtc_time.weekday < 7) ? WEEKDAY_NAMES[rtc_time.weekday] : "Unknown";
    log_i("RTC Time : 20%02d-%02d-%02d %02d:%02d:%02d (%s)",
          rtc_time.year,
          rtc_time.month,
          rtc_time.day,
          rtc_time.hour,
          rtc_time.minute,
          rtc_time.second,
          weekday_str);

    log_i("RTC Temp : %d C", temp);
    log_i("RTC Batt : %s", bat_low ? "Low Voltage! (Replace Battery)" : "Normal");
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), rtc_get, shell_rtc_get, Get current RTC time temperature and battery status);

/**
 * @brief 测试指令：设置当前 RTC 时间
 * 格式：rtc_set YYYY-MM-DD HH:MM:SS
 */
static void shell_rtc_set(int argc, char *argv[])
{
    sd3078_time_t rtc_time = {0};
    int           year     = 0;
    int           month    = 0;
    int           day      = 0;
    int           hour     = 0;
    int           minute   = 0;
    int           second   = 0;
    bsp_status_t  status   = BSP_OK;

    if (argc < 3)
    {
        log_e("Usage: rtc_set YYYY-MM-DD HH:MM:SS");
        log_e("Example: rtc_set 2026-07-01 15:30:00");
        return;
    }

    if (sscanf(argv[1], "%d-%d-%d", &year, &month, &day) != 3 ||
        sscanf(argv[2], "%d:%d:%d", &hour, &minute, &second) != 3)
    {
        log_e("Invalid format! Use: YYYY-MM-DD HH:MM:SS");
        return;
    }

    /* 参数范围简易校验 */
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
    {
        log_e("Invalid parameter range!");
        return;
    }

    /* 转换到 RTC 结构体数据范围 */
    rtc_time.year    = (uint8_t)(year - 2000);
    rtc_time.month   = (uint8_t)month;
    rtc_time.day     = (uint8_t)day;
    rtc_time.hour    = (uint8_t)hour;
    rtc_time.minute  = (uint8_t)minute;
    rtc_time.second  = (uint8_t)second;
    rtc_time.weekday = get_weekday_by_date((uint16_t)year, (uint8_t)month, (uint8_t)day);

    /* 写入硬件 */
    status = dev_sd3078_set_time(&rtc_time);
    if (status == BSP_OK)
    {
        log_i("RTC time updated successfully.");
        shell_rtc_get();
    }
    else
    {
        log_e("Failed to set RTC time! ret = %d", status);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), rtc_set, shell_rtc_set, Set current RTC time [YYYY-MM-DD HH:MM:SS]);

/**
 * @brief 测试指令：读取并输出 RTC 芯片内部温度
 */
static void shell_rtc_temp(void)
{
    int8_t       temp   = 0;
    bsp_status_t status = dev_sd3078_get_temperature(&temp);
    if (status == BSP_OK)
    {
        log_i("RTC Internal Temperature: %d C", temp);
    }
    else
    {
        log_e("Failed to read RTC temperature! ret = %d", status);
    }
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), rtc_temp, shell_rtc_temp, Get RTC internal temperature);

/**
 * @brief 测试指令：读取并 16 进制 Dump 所有 70 字节的内置用户 SRAM
 */
static void shell_rtc_sram_dump(void)
{
    uint8_t      ram_buf[SD3078_SRAM_SIZE_MAX] = {0};
    bsp_status_t status                        = BSP_OK;

    status = dev_sd3078_read_sram(0, ram_buf, SD3078_SRAM_SIZE_MAX);
    if (status != BSP_OK)
    {
        log_e("Failed to read RTC User SRAM! ret = %d", status);
        return;
    }

    log_i("SD3078 User SRAM 16-Hex Dump (Offset 2CH~71H):");
    log_i("-----------------------------------------------------------------");
    for (uint8_t i = 0; i < SD3078_SRAM_SIZE_MAX; i += 10)
    {
        log_i("  0x%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
              SD3078_REG_SRAM_START + i,
              ram_buf[i], ram_buf[i + 1], ram_buf[i + 2], ram_buf[i + 3], ram_buf[i + 4],
              ram_buf[i + 5], ram_buf[i + 6], ram_buf[i + 7], ram_buf[i + 8], ram_buf[i + 9]);
    }
    log_i("-----------------------------------------------------------------");
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), rtc_sram_dump, shell_rtc_sram_dump, Hex dump SD3078 70-Byte User SRAM);
