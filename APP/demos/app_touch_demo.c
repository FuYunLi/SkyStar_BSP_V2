/**
 * @file app_touch_demo.c
 * @brief FT6336 电容触摸演示与自检模块实现
 */

#include "app_touch_demo.h"
#include "dev_ft6336.h"
#include "bsp_logger.h"
#include "port_tick.h"
#include "shell.h"

#define LOG_TAG "APP_TOUCH"

/**
 * @brief 初始化触摸演示模块
 * @retval BSP_OK 初始化成功
 * @retval 其他 错误码
 */
bsp_status_t app_touch_demo_init(void)
{
    bsp_status_t ret = dev_ft6336_init(&touch_dev);
    if (ret == BSP_OK)
    {
        log_i("FT6336 touch device initialized successfully.");
    }
    else
    {
        log_e("FT6336 touch device initialization failed! Code: %d", ret);
    }
    
    return ret;
}

/**
 * @brief Shell 指令：轮询读取 FT6336 触摸点坐标，持续 20 秒
 */
static void shell_touch_poll(void)
{
    log_i("Start polling FT6336 touch coordinates for 20 seconds...");
    log_i("Move your finger on the screen to see raw points.");
    log_i("--------------------------------------------------");
    
    for (uint32_t i = 0; i < 200; i++)
    {
        bsp_status_t ret = dev_ft6336_read_touch(&touch_dev);
        if (ret == BSP_OK)
        {
            if (touch_dev.touch_count > 0)
            {
                log_i("Fingers: %d | P1: id=%d, ev=%d, (%d, %d) | P2: id=%d, ev=%d, (%d, %d)",
                      touch_dev.touch_count,
                      touch_dev.points[0].touch_id,
                      touch_dev.points[0].event,
                      touch_dev.points[0].x,
                      touch_dev.points[0].y,
                      touch_dev.points[1].touch_id,
                      touch_dev.points[1].event,
                      touch_dev.points[1].x,
                      touch_dev.points[1].y);
            }
        }
        else
        {
            log_e("Failed to read touch coordinates!");
            break;
        }
        
        port_tick_delay_ms(100);
    }
    
    log_i("--------------------------------------------------");
    log_i("Touch polling complete.");
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, 
                 touch_poll, shell_touch_poll, "Poll FT6336 touch coordinates for 20s");
