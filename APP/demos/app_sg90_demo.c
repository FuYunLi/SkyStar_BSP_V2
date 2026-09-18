/**
 * @file app_sg90_demo.c
 * @brief SG90 舵机控制自检演示实现
 * @note 硬件前置条件：SW7 拨码 BIT5 置于"双舵机"位（PB14/PB15 路由至
 *       舵机座）。sweep 指令在 Shell 上下文阻塞执行（与 ec11_monitor 同例），
 *       每步 15ms 给予舵机机械跟随时间。
 */

#define LOG_TAG "APP_SG90"

#include "app_sg90_demo.h"
#include "bsp_logger.h"
#include "bsp_board.h"
#include "dev_sg90.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define SG90_DEMO_SWEEP_STEP_DEG (5U)    /* 扫掠步进角度 */
#define SG90_DEMO_SWEEP_DELAY_MS (100U)  /* 步进保持时间，兼顾机械跟随 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 解析舵机通道参数（"1"/"2"，缺省为 1）
 */
static dev_sg90_id_t s_parse_id(const char *str)
{
    if ((str != NULL) && (str[0] == '2'))
    {
        return DEV_SG90_2;
    }

    return DEV_SG90_1;
}

/**
 * @brief sg90 Shell 指令入口：设置角度
 */
static int shell_sg90(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_i("usage: sg90 <0-180> [1|2]");
        return -1;
    }

    int angle = atoi(argv[1]);
    dev_sg90_id_t id = s_parse_id((argc >= 3) ? argv[2] : NULL);

    bsp_status_t ret = dev_sg90_set_angle(id, (uint8_t)((angle < 0) ? 0 : angle));
    if (ret != BSP_OK)
    {
        log_e("sg90 set failed! ret = %d", ret);
        return -1;
    }

    log_i("SG90-%d angle = %d", (int)id + 1, angle);
    return 0;
}

/**
 * @brief sg90_sweep Shell 指令入口：0→180→0 扫掠
 */
static int shell_sg90_sweep(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    dev_sg90_id_t id = DEV_SG90_1;
    log_i("SG90 sweep on channel %d...", (int)id + 1);

    /* 正程 0→180°：每步停留给机械跟随留时间 */
    for (int angle = 0; angle <= 180; angle += SG90_DEMO_SWEEP_STEP_DEG)
    {
        (void)dev_sg90_set_angle(id, (uint8_t)angle);
        bsp_tick_delay_ms(SG90_DEMO_SWEEP_DELAY_MS);
    }

    /* 回程 180→0° */
    for (int angle = 180; angle >= 0; angle -= SG90_DEMO_SWEEP_STEP_DEG)
    {
        (void)dev_sg90_set_angle(id, (uint8_t)angle);
        bsp_tick_delay_ms(SG90_DEMO_SWEEP_DELAY_MS);
    }

    log_i("SG90 sweep done.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化舵机控制演示模块
 */
bsp_status_t app_sg90_demo_init(void)
{
    bsp_status_t ret = dev_sg90_init(DEV_SG90_1);
    if (ret != BSP_OK)
    {
        log_e("SG90-1 init failed! ret = %d", ret);
        return ret;
    }

    ret = dev_sg90_init(DEV_SG90_2);
    if (ret != BSP_OK)
    {
        log_e("SG90-2 init failed! ret = %d", ret);
        return ret;
    }

    log_i("SG90 Demo loaded. Try: sg90 <0-180> / sg90_sweep");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, sg90, shell_sg90, Set servo angle (0-180, channel optional));
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, sg90_sweep, shell_sg90_sweep, Sweep servo 0-180-0 degrees);
