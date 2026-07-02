/**
 * @file app_ec11_demo.h
 * @brief EC11 旋转编码器 Shell 交互演示模块头文件
 * @note 导出 EC11 自检调试 Shell 命令接口。
 */

#ifndef __APP_EC11_DEMO_H
#define __APP_EC11_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化编码器 Shell 自检演示模块
 * @return bsp_status_t 执行状态码
 */
bsp_status_t app_ec11_demo_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_EC11_DEMO_H */
