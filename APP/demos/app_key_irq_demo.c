/**
 * @file app_key_irq_demo.c
 * @brief 按键 EXTI 中断计数自检演示实现
 * @note 对标 RocketPi 03_rocketpi_key_irq：EXTI 下降沿统计 KEY1/KEY2/KEY3 按下次数。
 *       板级按键为外部上拉、按下接地，故触发沿选下降沿。
 *       与 MultiButton 轮询按键 (dev_key) 并存，通过软开关隔离，避免同一次
 *       按键被中断计数与轮询双路同时响应。
 */

#define LOG_TAG "APP_KEY_IRQ"

#include "app_key_irq_demo.h"
#include "port_gpio.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 私有变量
 * ================================================================ */

/* 软开关：仅使能后 ISR 才累计计数 */
static volatile bool s_irq_armed = false;

/* 按键中断计数，ISR 上下文写入，主上下文读取 */
static volatile uint32_t s_irq_count[3] = {0U, 0U, 0U};

/* 按键逻辑引脚与显示名映射 */
static const port_gpio_id_t s_key_pins[3] = {PORT_GPIO_KEY1, PORT_GPIO_KEY2, PORT_GPIO_KEY3};
static const char *const s_key_names[3] = {"KEY1", "KEY2", "KEY3"};

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief KEY1 中断回调
 * @note ISR 上下文仅做计数自增，严禁耗时操作；消抖由 Shell 侧观察容忍
 */
static void s_key1_irq_cb(void)
{
    if (s_irq_armed)
    {
        s_irq_count[0]++;
    }
}

/**
 * @brief KEY2 中断回调
 */
static void s_key2_irq_cb(void)
{
    if (s_irq_armed)
    {
        s_irq_count[1]++;
    }
}

/**
 * @brief KEY3 中断回调
 */
static void s_key3_irq_cb(void)
{
    if (s_irq_armed)
    {
        s_irq_count[2]++;
    }
}

static port_exti_callback_t const s_key_irq_cbs[3] = {s_key1_irq_cb, s_key2_irq_cb, s_key3_irq_cb};

/**
 * @brief key_irq_arm Shell 指令入口：使能或关闭中断计数
 */
static int shell_key_irq_arm(int argc, char *argv[])
{
    bool arm = true;

    if (argc == 2)
    {
        arm = (atoi(argv[1]) != 0);
    }

    s_irq_armed = arm;
    log_i("KEY EXTI counting %s.", arm ? "armed" : "disarmed");

    return 0;
}

/**
 * @brief key_irq_count Shell 指令入口：查询或复位中断计数
 */
static int shell_key_irq_count(int argc, char *argv[])
{
    if ((argc == 2) && (strcmp(argv[1], "reset") == 0))
    {
        for (uint32_t i = 0U; i < 3U; i++)
        {
            s_irq_count[i] = 0U;
        }

        log_i("KEY IRQ counters reset.");
        return 0;
    }

    for (uint32_t i = 0U; i < 3U; i++)
    {
        log_i("%s irq count = %lu", s_key_names[i], (unsigned long)s_irq_count[i]);
    }

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化按键 EXTI 中断演示模块
 */
bsp_status_t app_key_irq_demo_init(void)
{
    for (uint32_t i = 0U; i < 3U; i++)
    {
        bsp_status_t ret = port_gpio_exti_init(s_key_pins[i], PORT_EXTI_TRIGGER_FALLING, s_key_irq_cbs[i]);
        if (ret != BSP_OK)
        {
            log_e("%s EXTI init failed! ret = %d", s_key_names[i], ret);
            return ret;
        }
    }

    log_i("KEY IRQ Demo loaded. Default disarmed, use 'key_irq_arm 1' to enable.");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, key_irq_arm, shell_key_irq_arm, Arm or disarm KEY EXTI counting);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, key_irq_count, shell_key_irq_count, Show or reset KEY IRQ counters);
