#ifndef __DEV_SD3078_H
#define __DEV_SD3078_H

#include "bsp_board.h"
#include "port_gpio.h"
#include "port_i2c.h"
#include <stdbool.h>


/* I2C 7位地址 */
#define SD3078_I2C_ADDR_7BIT 0x32
#define SD3078_I2C_ADDR_8BIT (SD3078_I2C_ADDR_7BIT << 1)

/* SD3078 小时寄存器 (02H/09H) 位定义 */
#define SD3078_HOUR_MODE_BIT (7U)                                    /* D7: 12/24小时制选择位 */
#define SD3078_HOUR_MODE_24H ((uint8_t)(1U << SD3078_HOUR_MODE_BIT)) /* 1 = 24H 模式 */
#define SD3078_HOUR_MODE_12H ((uint8_t)(0U << SD3078_HOUR_MODE_BIT)) /* 0 = 12H 模式 */

#define SD3078_HOUR_AM_PM_BIT (5U) /* D5: AM/PM 标志位 (仅12H模式有效) */
#define SD3078_HOUR_PM_FLAG   ((uint8_t)(1U << SD3078_HOUR_AM_PM_BIT))
#define SD3078_HOUR_AM_FLAG   ((uint8_t)(0U << SD3078_HOUR_AM_PM_BIT))

#define SD3078_HOUR_VALUE_MASK_24H (0x3FU) /* 24H模式下的小时值掩码 (D5~D0) */
#define SD3078_HOUR_VALUE_MASK_12H (0x1FU) /* 12H模式下的小时值掩码 (D4~D0) */

/**
 * @brief SD3078 时间与日期结构体

 */
typedef struct
{
    uint8_t year;    /* 0~99 (代表2000~2099) */
    uint8_t month;   /* 1~12 */
    uint8_t day;     /* 1~31 */
    uint8_t weekday; /* 0~6 */
    uint8_t hour;    /* 0~23 (24小时制) */
    uint8_t minute;  /* 0~59 */
    uint8_t second;  /* 0~59 */
} sd3078_time_t;

/**
 * @brief 报警模式掩码定义 (控制 0EH 报警允许寄存器)
 */
typedef enum
{
    SD3078_ALARM_NONE    = 0x00,
    SD3078_ALARM_SEC     = 0x01, // 秒一致报警
    SD3078_ALARM_MIN     = 0x02, // 分一致报警
    SD3078_ALARM_HOUR    = 0x04, // 时一致报警
    SD3078_ALARM_DAY     = 0x08, // 日一致报警
    SD3078_ALARM_WEEKDAY = 0x10, // 周一致报警
    SD3078_ALARM_MONTH   = 0x20, // 月一致报警
    SD3078_ALARM_YEAR    = 0x40, // 年一致报警
    SD3078_ALARM_ALL     = 0x7F  // 绝对精确匹配报警
} sd3078_alarm_mask_t;

/**
 * @brief 倒计时频率枚举 (控制 10H 寄存器 TDS)
 */
typedef enum
{
    SD3078_TIMER_FREQ_4096HZ = 0x00,
    SD3078_TIMER_FREQ_64HZ   = 0x01,
    SD3078_TIMER_FREQ_1HZ    = 0x02,
    SD3078_TIMER_FREQ_1_60HZ = 0x03 // 1分钟1次
} sd3078_timer_freq_t;

/* 核心 API */
bsp_status_t dev_sd3078_init(void);
bsp_status_t dev_sd3078_disable_charging(void);
bsp_status_t dev_sd3078_get_time(sd3078_time_t *time);
bsp_status_t dev_sd3078_set_time(const sd3078_time_t *time);
bsp_status_t dev_sd3078_get_temperature(int8_t *temperature);
bsp_status_t dev_sd3078_get_battery_status(bool *is_low_voltage);

/* 报警与倒计时定时器 (Alarm & Countdown) */
bsp_status_t dev_sd3078_set_alarm(const sd3078_time_t *alarm_time, uint8_t mask_flags);
bsp_status_t dev_sd3078_clear_alarm_flag(void);

bsp_status_t dev_sd3078_set_countdown(uint8_t init_val, sd3078_timer_freq_t freq);
bsp_status_t dev_sd3078_clear_countdown_flag(void);

/* SRAM 读写 (User RAM: 2CH~71H, 共 70 字节) */
#define SD3078_REG_SRAM_START  0x2C /* 2CH: 用户 RAM 起始 */
#define SD3078_SRAM_SIZE_MAX   70   /* 用户 RAM 总长度 70 字节 */
bsp_status_t dev_sd3078_read_sram(uint8_t offset, uint8_t *data, uint8_t len);
bsp_status_t dev_sd3078_write_sram(uint8_t offset, uint8_t *data, uint8_t len);

/* EXTI 中断挂载回调 */
bsp_status_t dev_sd3078_int_attach(port_exti_callback_t cb);

#endif /* __DEV_SD3078_H */
