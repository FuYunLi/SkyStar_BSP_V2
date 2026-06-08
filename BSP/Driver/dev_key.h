/**
 * @file dev_key.h
 * @brief 按键驱动头文件
 * @note 提供按键的初始化与按键事件回调绑定接口。
 */

#ifndef __DEV_KEY_H
#define __DEV_KEY_H

#include "bsp_board.h"
#include "multi_button.h"
#include "port_gpio.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 按键 ID 定义 */
typedef enum
{
    DEV_KEY1 = 0,
    DEV_KEY2,
    DEV_KEY3,
    DEV_KEY_MAX,
} dev_key_id_t;

void dev_key_init(void);
void dev_key_scan(void);
uint8_t dev_key_read(dev_key_id_t key_id);
void dev_key_attach(dev_key_id_t key_id, ButtonEvent event, BtnCallback cb);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_KEY_H */