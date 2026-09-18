/**
 * @file app_standby_demo.c
 * @brief 待机低功耗自检演示实现
 * @note 对标 RocketPi 39_rocketpi_standby_wkup。要点：
 *       1. STANDBY 是最深低功耗档：1.8V 内核域断电，SRAM 与寄存器内容
 *          全部丢失，唤醒后等同冷复位从头执行；
 *       2. 唤醒源：WKUP 引脚（PA0，即本板 KEY1）上升沿、RTC 闹钟、
 *          IWDG 复位等；
 *       3. 复位来源判定：PWR 待机标志 SBF 在唤醒后保持置位，软件读取
 *          后须手动清除，借此区分"上电冷启动"与"待机唤醒"。
 */

#define LOG_TAG "APP_STANDBY"

#include "app_standby_demo.h"
#include "bsp_logger.h"
#include "stm32f4xx_hal.h"
#include "port_gpio.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define STANDBY_DEMO_ENTER_DELAY_MS (3000U) /* 进 standby 前的缓冲时间，留出日志输出窗口 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief standby_enter Shell 指令入口：进入 STANDBY 模式
 * @note 唤醒方式：KEY1(PA0/WKUP) 拉高（上升沿）。
 *       进入后 SRAM/寄存器全部掉电，程序从头重启，不会回到本函数。
 */
static int shell_standby_enter(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    log_w("Entering STANDBY in %u ms. Wake up by KEY1 (PA0/WKUP rising edge)...",
          (unsigned int)STANDBY_DEMO_ENTER_DELAY_MS);
    bsp_tick_delay_ms(STANDBY_DEMO_ENTER_DELAY_MS);

    /* PWR 外设时钟：访问 PWR 寄存器的前置条件 */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* 清除历史唤醒标志，防止遗留 WUF 导致立即唤醒 */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    /* 使能 WKUP 引脚（PA0，上升沿有效），并触发清唤醒挂起 */
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    log_i("Goodbye. Reset to wake.");

    /* 进入 STANDBY：内核域断电，本函数不会返回 */
    HAL_PWR_EnterSTANDBYMode();

    /* 仅编译器可见的兜底返回（正常不可达） */
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化待机低功耗演示模块
 */
bsp_status_t app_standby_demo_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();

    /* 复位来源判定：SBF 置位说明本次重启来自 STANDBY 唤醒而非上电 */
    if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET)
    {
        log_i("Resumed from STANDBY (cold restart). Wake source: WKUP pin.");
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
    }
    else
    {
        log_i("Normal power-on reset.");
    }

    log_i("Standby Demo loaded. Try: standby_enter");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, standby_enter, shell_standby_enter, Enter STANDBY mode (wake by KEY1/PA0));
