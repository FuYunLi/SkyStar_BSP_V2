/**
 * @file dev_ft6336.h
 * @brief FT6336 电容触摸芯片底层物理驱动头文件
 * @note 封装基于软件 I2C 总线与 GPIO 中断引脚的电容触摸坐标读取接口
 */

#ifndef __DEV_FT6336_H
#define __DEV_FT6336_H

#include "bsp_board.h"
#include "port_i2c.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FT6336 I2C 从机地址定义 */
#define FT6336_I2C_ADDR_7BIT  0x38
#define FT6336_I2C_ADDR_8BIT  (FT6336_I2C_ADDR_7BIT << 1)

/* FT6336 最大支持触摸点数 */
#define FT6336_MAX_TOUCH_POINTS 2

/**
 * @brief 触摸事件类型枚举
 */
typedef enum
{
    FT6336_EVENT_PRESS_DOWN = 0, /* 按下 */
    FT6336_EVENT_LIFT_UP    = 1, /* 抬起 */
    FT6336_EVENT_CONTACT    = 2, /* 接触移动 */
    FT6336_EVENT_NONE       = 3  /* 无有效事件 */
} ft6336_touch_event_t;

/**
 * @brief 单点触摸信息数据结构
 */
typedef struct
{
    bool                 valid;      /* 触摸点数据是否有效 */
    uint8_t              touch_id;   /* 触摸ID追踪 (0 或 1) */
    ft6336_touch_event_t event;      /* 触摸事件标志 */
    uint16_t             x;          /* 触摸点 X 坐标 */
    uint16_t             y;          /* 触摸点 Y 坐标 */  
} ft6336_touch_point_t;

/**
 * @brief FT6336 设备句柄结构体
 */
typedef struct
{
    port_i2c_id_t        i2c_bus;                                /* 绑定的软件 I2C 总线 ID */
    uint8_t              touch_count;                            /* 当前有效的触摸点数 */
    ft6336_touch_point_t points[FT6336_MAX_TOUCH_POINTS];        /* 触摸点详情数组 */
} ft6336_dev_t;

/* 声明板级全局默认触摸实例 */
extern ft6336_dev_t touch_dev;

/* 导出驱动层核心 API 声明 */
bsp_status_t dev_ft6336_init(ft6336_dev_t *dev);
bsp_status_t dev_ft6336_read_touch(ft6336_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_FT6336_H */
