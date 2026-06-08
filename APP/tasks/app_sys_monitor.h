/**
 * @file app_sys_monitor.h
 * @brief 系统状态监视器任务
 * @note 实现基于有限状态机(FSM)的系统状态管理，解耦输入事件与外设输出。
 */

#ifndef __APP_SYS_MONITOR_H
#define __APP_SYS_MONITOR_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 系统运行状态定义 */
typedef enum {
    SYS_STATE_IDLE = 0,     /*!< 待机状态：核心板 LED 慢闪，RGB 待机/呼吸 */
    SYS_STATE_WORK,         /*!< 工作状态：核心板 LED 常亮，RGB 跑马灯/流光 */
    SYS_STATE_ALARM,        /*!< 报警状态：核心板 LED 极速闪烁，RGB 红光急闪，蜂鸣器间歇鸣叫 */
    SYS_STATE_MAX
} sys_state_t;

/* 系统输入事件定义 */
typedef enum {
    SYS_EVENT_NONE = 0,     /*!< 无事件 */
    SYS_EVENT_NEXT,         /*!< 切换下一状态 (IDLE <-> WORK) */
    SYS_EVENT_ALARM,        /*!< 触发/解除报警状态 */
    SYS_EVENT_MUTE_TOGGLE,  /*!< 切换静音模式 */
    SYS_EVENT_MAX
} sys_event_t;

/**
 * @brief  初始化系统状态监视器任务
 * @return bsp_status_t BSP_OK 成功
 */
bsp_status_t app_sys_monitor_init(void);

/**
 * @brief  向状态机发布系统事件
 * @param  event 要发布的事件
 */
void app_sys_monitor_post_event(sys_event_t event);

/**
 * @brief  获取当前系统状态
 * @return sys_state_t 当前状态
 */
sys_state_t app_sys_monitor_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_SYS_MONITOR_H */
