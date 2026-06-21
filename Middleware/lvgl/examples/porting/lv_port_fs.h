/**
 * @file lv_port_fs.h
 * @brief LVGL 9.3.0 文件系统接口移植层头文件
 */

#ifndef LV_PORT_FS_H
#define LV_PORT_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/**
 * @brief 初始化 LVGL 文件系统接口并向系统注册文件系统设备
 */
void lv_port_fs_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_PORT_FS_H */
