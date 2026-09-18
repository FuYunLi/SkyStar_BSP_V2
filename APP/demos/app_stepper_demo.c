/**
 * @file app_stepper_demo.c
 * @brief TMC2209 步进电机自检演示实现
 * @note 对标官方出厂 STEPPER 例程。【未验证】步进电机尚未接线实测。
 *       SW7 BIT8 需拨至板载步进驱动位。
 */

#define LOG_TAG "APP_STEPPER"

#include "app_stepper_demo.h"
#include "bsp_logger.h"
#include "dev_stepper.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief stepper_enable Shell 指令入口：使能/禁用驱动
 */
static int shell_stepper_enable(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: stepper_enable <0|1>");
        return -1;
    }

    bool enable = (atoi(argv[1]) != 0);
    bsp_status_t ret = dev_stepper_enable(enable);
    if (ret != BSP_OK)
    {
        return -1;
    }

    log_i("stepper %s.", enable ? "enabled" : "disabled");
    return 0;
}

/**
 * @brief stepper_move Shell 指令入口：按步数与频率移动
 */
static int shell_stepper_move(int argc, char *argv[])
{
    if (argc != 4)
    {
        log_i("usage: stepper_move <steps> <freq_hz> <0|1 dir>");
        return -1;
    }

    uint32_t steps = (uint32_t)atoi(argv[1]);
    uint32_t freq = (uint32_t)atoi(argv[2]);
    bool cw = (atoi(argv[3]) != 0);

    (void)dev_stepper_set_dir(cw);

    bsp_status_t ret = dev_stepper_move_steps(steps, freq);
    if (ret != BSP_OK)
    {
        log_e("move failed! ret = %d", ret);
        return -1;
    }

    log_i("moved ~%s steps at %s Hz (%s).", argv[1], argv[2], cw ? "CW" : "CCW");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化步进电机演示模块
 */
bsp_status_t app_stepper_demo_init(void)
{
    bsp_status_t ret = dev_stepper_init();
    if (ret != BSP_OK)
    {
        log_e("stepper init failed! ret = %d", ret);
        return ret;
    }

    log_i("Stepper Demo loaded. Try: stepper_enable 1 / stepper_move 3200 2000 1");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, stepper_enable, shell_stepper_enable, Enable or disable stepper driver);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, stepper_move, shell_stepper_move, Move stepper (blocking, steps freq dir));
