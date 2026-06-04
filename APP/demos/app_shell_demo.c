/**
 * @file app_shell_demo.c
 * @brief Letter Shell 使用演示层源文件
 * @note 本文件仅作为演示模块接口骨架，具体命令导出和功能实现留给用户实操练习。
 */

#include "app_shell_demo.h"
#include "shell.h"
#include <stdio.h>

void shell_print_test(void)
{
    printf("=================================\r\n");
    printf("SkyStar System running...\r\n");
    printf("Build Time: %s %s\r\n", __DATE__, __TIME__);
    printf("MCU: STM32F407VET6\r\n");
    printf("=================================\r\n");
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, print_test, shell_print_test, "print test");

/**
 * @brief 初始化 Letter Shell 演示模块
 * @return bsp_status_t 初始化状态，成功返回 BSP_OK
 */
bsp_status_t app_shell_demo_init(void)
{
    /* 静态注册时，这里只需打印一条模块启动日志 */
    printf("[APP] Shell demo module initialized successfully\r\n");
    return BSP_OK;
}
