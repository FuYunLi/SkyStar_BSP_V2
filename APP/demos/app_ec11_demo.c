/**
 * @file app_ec11_demo.c
 * @brief EC11 旋转编码器 Shell 自检演示模块实现
 * @note 导出 ec11_read 与 ec11_monitor 两个 Shell 控制台调试命令。
 */

#define LOG_TAG "APP_EC11"

#include "app_ec11_demo.h"
#include "bsp_ec11.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>

/* ================================================================
 * 私有函数声明与实现
 * ================================================================ */

/**
 * @brief 读取单次 EC11 状态数据并打印
 */
static void s_ec11_read_once(void)
{
    bsp_ec11_info_t info = {0};
    bsp_status_t status = bsp_ec11_get_info(&info);
    if (status == BSP_OK)
    {
        log_i("EC11 Status: Count = %d, Dir = %s",
              info.count,
              (info.dir > 0) ? "CW" : ((info.dir < 0) ? "CCW" : "NONE"));
    }
    else
    {
        log_e("Failed to read EC11 info! ret = %d", status);
    }
}

/**
 * @brief ec11_read Shell 调试指令入口
 * @param argc 参数个数
 * @param argv 参数列表
 * @return int 执行状态
 */
static int shell_ec11_read(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    s_ec11_read_once();
    return 0;
}

/**
 * @brief ec11_monitor Shell 调试指令入口，实现周期监测与变化回显
 * @param argc 参数个数
 * @param argv 参数列表
 * @return int 执行状态
 */
static int shell_ec11_monitor(int argc, char *argv[])
{
    int check_count = 100; /* 默认执行 100 次检测（约 10 秒） */

    if (argc == 2)
    {
        check_count = atoi(argv[1]);
        if (check_count <= 0)
        {
            log_e("Invalid count! Must be greater than 0.");
            return -1;
        }
        if (check_count > 1000)
        {
            log_w("Count %d is too large, cap to 1000.", check_count);
            check_count = 1000;
        }
    }

    log_i("Start monitoring EC11 encoder (total %d checks, 100ms interval)...", check_count);

    bsp_ec11_info_t last_info = {0};
    /* 获取初值，防止因初始化前残留值导致瞬时触发打印 */
    bsp_ec11_get_info(&last_info);

    for (int i = 0; i < check_count; i++)
    {
        bsp_ec11_info_t curr_info = {0};
        bsp_status_t status = bsp_ec11_get_info(&curr_info);
        if (status == BSP_OK)
        {
            /* 仅在物理旋转计数值发生改变时进行日志输出 */
            if (curr_info.count != last_info.count)
            {
                log_i("EC11 Rotate: Count = %d, Dir = %s",
                      curr_info.count,
                      (curr_info.dir > 0) ? "CW" : "CCW");
                last_info = curr_info;
            }
        }
        else
        {
            log_e("Error reading EC11! ret = %d", status);
            break;
        }
        HAL_Delay(100);
    }

    log_i("EC11 monitoring finished.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化编码器 Shell 自检演示模块
 */
bsp_status_t app_ec11_demo_init(void)
{
    log_i("EC11 Shell Demo loaded.");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ec11_read, shell_ec11_read, Read EC11 count once);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, ec11_monitor, shell_ec11_monitor, Monitor EC11 rotation interactively);
