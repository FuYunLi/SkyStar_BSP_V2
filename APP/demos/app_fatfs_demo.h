/**
 * @file app_fatfs_demo.h
 * @brief FatFS 文件系统功能演示与自检头文件
 */

#ifndef APP_FATFS_DEMO_H
#define APP_FATFS_DEMO_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 FatFS 演示模块
 * @note 若启动时检测到 SD 卡物理在位，则自动尝试挂载文件系统
 * @return bsp_status_t
 */
bsp_status_t app_fatfs_demo_init(void);

/**
 * @brief 运行完整的 FatFS 自动化读写与性能测试自检
 * @return bsp_status_t
 */
bsp_status_t app_fatfs_test_run(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FATFS_DEMO_H */
