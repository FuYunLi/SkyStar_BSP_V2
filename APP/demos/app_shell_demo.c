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
#include "dev_led.h"
#include "dev_buzzer.h"
#include "port_i2c.h"
#define LOG_TAG "SHELL_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>

void shell_print_test(void)
{
    log_i("=================================");
    log_i("SkyStar System running...");
    log_i("Build Time: %s %s", __DATE__, __TIME__);
    log_i("MCU: STM32F407VET6");
    log_i("=================================");
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, print, shell_print_test, "print test");

void shell_delay_test(void)
{
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;

    log_i("--- Delay Precision Test via DWT (CYCCNT) ---");

    /* 1. 测试 100us DWT 精确阻塞延时 */
    uint32_t start_cycles = port_dwt_get_cycles();
    port_dwt_delay_us(100U);
    uint32_t end_cycles = port_dwt_get_cycles();
    uint32_t diff_cycles = end_cycles - start_cycles;
    uint32_t actual_us_x100 = (diff_cycles * 100U) / cycles_per_us;
    int32_t error_us_x100 = (int32_t)actual_us_x100 - 10000;

    log_i("100us DWT Delay: target=100.00us, actual=%lu.%02luus, diff=%s%ld.%02ldus (elapsed %lu cycles)",
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

    log_i("1ms Tick Delay: target=1000.00us, actual=%lu.%02luus, diff=%s%ld.%02ldus (elapsed %lu cycles)",
           (unsigned long)(actual_us_x100 / 100), (unsigned long)(actual_us_x100 % 100),
           (error_us_x100 >= 0) ? "+" : "-",
           (long)(error_us_x100 >= 0 ? error_us_x100 / 100 : -error_us_x100 / 100),
           (long)(error_us_x100 >= 0 ? error_us_x100 % 100 : -error_us_x100 % 100),
           (unsigned long)diff_cycles);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) | SHELL_CMD_DISABLE_RETURN, delay, shell_delay_test, "delay precision test");

int shell_gpio_test(int argc, char *argv[])
{
    if (argc < 3)
    {
        log_e("Usage: gpio <PIN_NAME> <HIGH|LOW|READ>");
        log_i("Example: gpio LED_CORE HIGH");
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
        log_e("Unknown pin name: %s", argv[1]);
        return -2;
    }

    port_gpio_state_t state = PORT_GPIO_LOW;
    if (strcmp(argv[2], "HIGH") == 0)
    {
        state = PORT_GPIO_HIGH;
        port_gpio_write(pin_id, state);
        log_i("Set pin %s to HIGH", argv[1]);
    }
    else if (strcmp(argv[2], "LOW") == 0)
    {
        state = PORT_GPIO_LOW;
        port_gpio_write(pin_id, state);
        log_i("Set pin %s to LOW", argv[1]);
    }
    else if (strcmp(argv[2], "READ") == 0)
    {
        port_gpio_read(pin_id, &state);
        log_i("Pin %s state: %s", argv[1], (state == PORT_GPIO_HIGH) ? "HIGH" : "LOW");
    }
    else
    {
        log_e("Unknown action: %s. Use HIGH, LOW or READ.", argv[2]);
        return -3;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, gpio, shell_gpio_test, "gpio test command");

int shell_led_test(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: led <on|off|toggle>");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        dev_led_set(LED_CORE,DEV_LED_ON);
        log_i("LED turned ON");
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        dev_led_set(LED_CORE,DEV_LED_OFF);
        log_i("LED turned OFF");
    }
    else if (strcmp(argv[1], "toggle") == 0)
    {
        dev_led_toggle(LED_CORE);
        log_i("LED toggled");
    }
    else
    {
        log_e("Unknown action: %s. Use on, off or toggle.", argv[1]);
        return -2;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, led, shell_led_test, "LED test command");

int shell_buzzer_test(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: buzzer <on|off|freq|tone> [param1] [param2]\r\n");
        printf("Examples:\r\n");
        printf("  buzzer on\r\n");
        printf("  buzzer off\r\n");
#if BUZZER_TYPE == BUZZER_PASSIVE
        printf("  buzzer freq 2000\r\n");
        printf("  buzzer tone 2500 50\r\n");
#endif
        return -1;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        dev_buzzer_on();
        log_i("Buzzer turned ON");
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        dev_buzzer_off();
        log_i("Buzzer turned OFF");
    }
#if BUZZER_TYPE == BUZZER_PASSIVE
    else if (strcmp(argv[1], "freq") == 0)
    {
        if (argc < 3)
        {
            log_e("Usage: buzzer freq <hz>");
            return -2;
        }
        int freq = 0;
        sscanf(argv[2], "%d", &freq);
        if (freq <= 0 || freq > 65535)
        {
            log_e("Invalid frequency: %d Hz", freq);
            return -3;
        }
        dev_buzzer_set_freq((uint16_t)freq);
        log_i("Buzzer frequency set to %d Hz", freq);
    }
    else if (strcmp(argv[1], "tone") == 0)
    {
        if (argc < 4)
        {
            log_e("Usage: buzzer tone <hz> <volume 0-100>");
            return -4;
        }
        int freq = 0;
        int vol = 0;
        sscanf(argv[2], "%d", &freq);
        sscanf(argv[3], "%d", &vol);
        if (freq <= 0 || freq > 65535)
        {
            log_e("Invalid frequency: %d Hz", freq);
            return -5;
        }
        if (vol < 0 || vol > 100)
        {
            log_e("Invalid volume: %d (0-100)", vol);
            return -6;
        }
        dev_buzzer_tone((uint16_t)freq, (uint8_t)vol);
        log_i("Buzzer playing tone: %d Hz, volume %d%%", freq, vol);
    }
#endif
    else
    {
        log_e("Unknown action: %s", argv[1]);
        return -7;
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, buzzer, shell_buzzer_test, "buzzer test command");

int shell_i2c_scan(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: i2c_scan <hw|sw>");
        return -1;
    }

    port_i2c_id_t id = PORT_I2C_1;
    if (strcmp(argv[1], "hw") == 0)
    {
        id = PORT_I2C_1;
    }
    else if (strcmp(argv[1], "sw") == 0)
    {
        id = PORT_I2C_SOFT;
    }
    else
    {
        log_e("Unknown bus: %s. Use hw or sw.", argv[1]);
        return -2;
    }

    // 确保扫描前总线初始化完成
    if (port_i2c_init(id) != BSP_OK)
    {
        log_e("Failed to initialize I2C bus %s", argv[1]);
        return -3;
    }

    log_i("Scanning I2C bus %s...", argv[1]);

    int devices_found = 0;
    uint8_t dummy_data = 0x00;

    for (uint8_t addr = 0x01; addr < 0x80; addr++)
    {
        // 使用 I2C 写接口探测器件响应，从机地址需左移 1 位
        bsp_status_t status = port_i2c_write(id, addr << 1, &dummy_data, 1, 10);
        if (status == BSP_OK)
        {
            log_i("Found device at address: 0x%02X (7-bit: 0x%02X)", addr << 1, addr);
            devices_found++;
        }
    }

    if (devices_found == 0)
    {
        log_w("No I2C devices found on bus %s.", argv[1]);
    }
    else
    {
        log_i("Scan complete. Found %d devices.", devices_found);
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, i2c_scan, shell_i2c_scan, "scan i2c devices on hw/sw bus");

/**
 * @brief 初始化 Letter Shell 演示模块
 * @return bsp_status_t 初始化状态，成功返回 BSP_OK
 */
bsp_status_t app_shell_demo_init(void)
{
    /* 静态注册时，这里只需打印一条模块启动日志 */
    log_i("[APP] Shell demo module initialized successfully");
    return BSP_OK;
}

