/**
 * @file app_hcsr04_demo.c
 * @brief HC-SR04 超声波测距自检演示实现
 * @note 对标 RocketPi 19_rocketpi_hcsr04。【未验证】外部模块尚未接线
 *       实测，示例接线 PD11(TRIG)/PA8(ECHO)，接线不同时修改
 *       port_gpio 映射表即可。
 */

#define LOG_TAG "APP_HCSR04"

#include "app_hcsr04_demo.h"
#include "bsp_logger.h"
#include "dev_hcsr04.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define HCSR04_DEMO_DEFAULT_TIMES (5U)  /* 默认测距次数 */
#define HCSR04_DEMO_MAX_TIMES     (20U) /* 次数上限 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief hcsr04 Shell 指令入口：连续测距并输出
 */
static int shell_hcsr04(int argc, char *argv[])
{
    uint32_t times = HCSR04_DEMO_DEFAULT_TIMES;

    if (argc == 2)
    {
        times = (uint32_t)atoi(argv[1]);
        if ((times == 0U) || (times > HCSR04_DEMO_MAX_TIMES))
        {
            times = HCSR04_DEMO_MAX_TIMES;
        }
    }

    /* 连续多次测量：单次超时不中断序列，超时通常意味着超出量程 */
    for (uint32_t i = 0U; i < times; i++)
    {
        uint32_t distance_mm = 0U;
        bsp_status_t ret = dev_hcsr04_read_mm(&distance_mm);

        if (ret == BSP_OK)
        {
            log_i("distance = %lu mm", (unsigned long)distance_mm);
        }
        else
        {
            log_w("measure %u timeout (no echo).", (unsigned int)(i + 1U));
        }

        bsp_tick_delay_ms(100);
    }

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化超声波测距演示模块
 */
bsp_status_t app_hcsr04_demo_init(void)
{
    log_i("HCSR04 Demo loaded. Try: hcsr04 [times]");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, hcsr04, shell_hcsr04, Measure distance by ultrasonic (mm, times optional));
