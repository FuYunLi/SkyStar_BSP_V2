/**
 * @file app_shell_demo.c
 * @brief Letter Shell 使用演示层源文件
 * @note 本文件仅作为演示模块接口骨架，具体命令导出和功能实现留给用户实操练习。
 */

#include "app_shell_demo.h"
#include "shell.h"
#include "port_tick.h"
#include "port_dwt.h"
#include "port_gpio.h"
#include <stdio.h>
#include <string.h>

void shell_print_test(void)
{
    printf("=================================\r\n");
    printf("SkyStar System running...\r\n");
    printf("Build Time: %s %s\r\n", __DATE__, __TIME__);
    printf("MCU: STM32F407VET6\r\n");
    printf("=================================\r\n");
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, print_test, shell_print_test, "print test");

void shell_delay_test(void)
{
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    printf("--- Delay Precision Test via DWT (CYCCNT) ---\r\n");

    /* 1. 测试 100us DWT 精确阻塞延时 */
    uint32_t start_cycles = port_dwt_get_cycles();
    port_dwt_delay_us(100U);
    uint32_t end_cycles = port_dwt_get_cycles();
    uint32_t diff_cycles = end_cycles - start_cycles;
    uint32_t actual_us_x100 = (diff_cycles * 100U) / cycles_per_us;
    int32_t error_us_x100 = (int32_t)actual_us_x100 - 10000;

    printf("100us DWT Delay: target=100.00us, actual=%lu.%02luus, diff=%s%ld.%02ldus (elapsed %lu cycles)\r\n",
           (unsigned long)(actual_us_x100 / 100), (unsigned long)(actual_us_x100 % 100),
           (error_us_x100 >= 0) ? "+" : "-",
           (long)(error_us_x100 >= 0 ? error_us_x100 / 100 : -error_us_x100 / 100),
           (long)(error_us_x100 >= 0 ? error_us_x100 % 100 : -error_us_x100 % 100),
           (unsigned long)diff_cycles);

    /* 2. 测试 1ms SysTick 阻塞延时 */
    start_cycles = port_dwt_get_cycles();
    port_tick_delay_ms(1U);
    end_cycles = port_dwt_get_cycles();
    diff_cycles = end_cycles - start_cycles;
    actual_us_x100 = (diff_cycles * 100U) / cycles_per_us;
    error_us_x100 = (int32_t)actual_us_x100 - 100000;

    printf("1ms Tick Delay: target=1000.00us, actual=%lu.%02luus, diff=%s%ld.%02ldus (elapsed %lu cycles)\r\n",
           (unsigned long)(actual_us_x100 / 100), (unsigned long)(actual_us_x100 % 100),
           (error_us_x100 >= 0) ? "+" : "-",
           (long)(error_us_x100 >= 0 ? error_us_x100 / 100 : -error_us_x100 / 100),
           (long)(error_us_x100 >= 0 ? error_us_x100 % 100 : -error_us_x100 % 100),
           (unsigned long)diff_cycles);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, delay_test, shell_delay_test, "delay precision test");

int shell_gpio_test(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: gpio_test <PIN_NAME> <HIGH|LOW|READ>\r\n");
        printf("Example: gpio_test LED_CORE HIGH\r\n");
        return -1;
    }

    port_gpio_id_t pin_id = PORT_GPIO_MAX;
    if (strcmp(argv[1], "LED_CORE") == 0)
    {
        pin_id = PORT_GPIO_LED_CORE;
    }
    else if (strcmp(argv[1], "KEY1") == 0)
    {
        pin_id = PORT_GPIO_KEY1;
    }
    else if (strcmp(argv[1], "KEY2") == 0)
    {
        pin_id = PORT_GPIO_KEY2;
    }
    else if (strcmp(argv[1], "KEY3") == 0)
    {
        pin_id = PORT_GPIO_KEY3;
    }
    else
    {
        printf("Unknown pin name: %s\r\n", argv[1]);
        return -2;
    }

    port_gpio_state_t state = PORT_GPIO_LOW;
    if (strcmp(argv[2], "HIGH") == 0)
    {
        state = PORT_GPIO_HIGH;
        port_gpio_write(pin_id, state);
        printf("Set pin %s to HIGH\r\n", argv[1]);
    }
    else if (strcmp(argv[2], "LOW") == 0)
    {
        state = PORT_GPIO_LOW;
        port_gpio_write(pin_id, state);
        printf("Set pin %s to LOW\r\n", argv[1]);
    }
    else if (strcmp(argv[2], "READ") == 0)
    {
        port_gpio_read(pin_id, &state);
        printf("Pin %s state: %s\r\n", argv[1], (state == PORT_GPIO_HIGH) ? "HIGH" : "LOW");
    }
    else
    {
        printf("Unknown action: %s. Use HIGH, LOW or READ.\r\n", argv[2]);
        return -3;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, gpio_test, shell_gpio_test, "gpio test command");

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
