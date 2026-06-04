/**
 * @file bsp_shell.h
 * @brief 板级 Shell 服务层头文件
 * @note 接入 Letter Shell 中间件，向外暴露 Shell 实例和轮询调度接口。
 */

#ifndef __BSP_SHELL_H
#define __BSP_SHELL_H

#include "bsp_board.h"
#include <stdint.h>

#include "shell.h"

/* Shell 实例 */
extern Shell shell;

bsp_status_t bsp_shell_init(void);
void bsp_shell_process(void);

#endif /* __BSP_SHELL_H */