/**
 * @file app_io_check_demo.c
 * @brief 扩展 IO 批量检查自检演示实现
 * @note 对标 RocketPi 42_rocketpi_extern_io_check。检测序列：
 *       全亮 1s → 全灭 → 流水灯 N 轮 → 全亮 1s → 全灭。
 *       Shell 指令上下文允许阻塞等待（与 ec11_monitor 同例），
 *       但不适用于定时回调场景。
 */

#define LOG_TAG "APP_IO_CHECK"

#include "app_io_check_demo.h"
#include "bsp_board.h"
#include "bsp_logger.h"
#include "bsp_led.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define IO_CHECK_DEFAULT_ROUNDS (3U)    /* 默认流水灯轮数 */
#define IO_CHECK_MAX_ROUNDS     (20U)   /* 轮数上限 */
#define IO_CHECK_STEP_MS        (100U)  /* 流水步进间隔 */
#define IO_CHECK_HOLD_MS        (1000U) /* 全亮/全灭保持时长 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 批量设置 8 颗扩展 LED
 */
static void s_io_led_set_all(bsp_led_state_t state)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_LED_MAX; i++)
    {
        (void)bsp_led_set((bsp_led_id_t)i, state);
    }
}

/**
 * @brief 流水灯一轮：点亮位从 LED1 逐个推进到 LED8
 */
static void s_io_led_flow_once(void)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_LED_MAX; i++)
    {
        (void)bsp_led_set((bsp_led_id_t)i, BSP_LED_ON);

        if (i > 0U)
        {
            (void)bsp_led_set((bsp_led_id_t)(i - 1U), BSP_LED_OFF);
        }

        bsp_tick_delay_ms(IO_CHECK_STEP_MS);
    }

    (void)bsp_led_set((bsp_led_id_t)(BSP_LED_MAX - 1U), BSP_LED_OFF);
}

/**
 * @brief io_check Shell 指令入口：执行批量 IO 检测序列
 */
static int shell_io_check(int argc, char *argv[])
{
    uint32_t rounds = IO_CHECK_DEFAULT_ROUNDS;

    if (argc == 2)
    {
        rounds = (uint32_t)atoi(argv[1]);
        if ((rounds == 0U) || (rounds > IO_CHECK_MAX_ROUNDS))
        {
            log_w("rounds %u out of range, clamp to %u", (unsigned int)rounds,
                  (unsigned int)IO_CHECK_MAX_ROUNDS);
            rounds = IO_CHECK_MAX_ROUNDS;
        }
    }

    log_i("IO check start: all-on -> flow x%u -> all-off", (unsigned int)rounds);

    /* 阶段 1：全亮保持，验证 8 路全部有效 */
    s_io_led_set_all(BSP_LED_ON);
    bsp_tick_delay_ms(IO_CHECK_HOLD_MS);

    /* 阶段 2：流水灯，验证逐路可控 */
    s_io_led_set_all(BSP_LED_OFF);
    for (uint32_t r = 0U; r < rounds; r++)
    {
        s_io_led_flow_once();
    }

    /* 阶段 3：全亮收尾并归零，验证关断能力 */
    s_io_led_set_all(BSP_LED_ON);
    bsp_tick_delay_ms(IO_CHECK_HOLD_MS);
    s_io_led_set_all(BSP_LED_OFF);

    log_i("IO check done.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化扩展 IO 批量检查演示模块
 */
bsp_status_t app_io_check_demo_init(void)
{
    log_i("IO Check Demo loaded. Try: io_check [rounds]");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, io_check, shell_io_check, Batch check 8 extended LEDs (all-on, flow, all-off));
