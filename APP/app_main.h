/**
 * @file app_main.h
 * @brief 应用层主入口头文件
 * @note 应用层初始化入口，注册 Demo 和 Task
 */

#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 应用层初始化入口
 * @note 注册常驻任务和 Demo，无 while(1)
 */
void app_main_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MAIN_H */
