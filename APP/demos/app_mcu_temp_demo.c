/**
 * @file app_mcu_temp_demo.c
 * @brief 片内温度采集自检演示实现
 * @note 对标 RocketPi 20_rocketpi_adc_mcu_temperature。温度传感器为
 *       高源阻抗弱信号源，单次采样抖动明显，本演示在应用层做 N 次
 *       采样算术平均——接口层保持单发单读的纯净语义，统计策略归应用层。
 */

#define LOG_TAG "APP_MCU_TEMP"

#include "app_mcu_temp_demo.h"
#include "bsp_logger.h"
#include "port_adc.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define TEMP_DEMO_DEFAULT_SAMPLES (8U)   /* 默认平均采样次数 */
#define TEMP_DEMO_MAX_SAMPLES     (100U) /* 采样次数上限 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief mcu_temp Shell 指令入口：多次采样平均后打印片内温度
 */
static int shell_mcu_temp(int argc, char *argv[])
{
    uint32_t times = TEMP_DEMO_DEFAULT_SAMPLES;

    if (argc == 2)
    {
        times = (uint32_t)atoi(argv[1]);
        if ((times == 0U) || (times > TEMP_DEMO_MAX_SAMPLES))
        {
            log_w("sample count %u out of range, clamp to %u", (unsigned int)times,
                  (unsigned int)TEMP_DEMO_MAX_SAMPLES);
            times = TEMP_DEMO_MAX_SAMPLES;
        }
    }

    /* 累计平均：单次读数抖动大，均值才能稳定到 ±1°C 量级 */
    uint32_t raw_sum = 0U;
    uint32_t valid = 0U;

    for (uint32_t i = 0U; i < times; i++)
    {
        uint32_t raw = 0U;
        if (port_adc_read_raw(PORT_ADC_CH_MCU_TEMP, &raw) == BSP_OK)
        {
            raw_sum += raw;
            valid++;
        }
    }

    if (valid == 0U)
    {
        log_e("temperature sampling failed!");
        return -1;
    }

    uint32_t raw_avg = raw_sum / valid;
    uint32_t vsense_mv = (raw_avg * PORT_ADC_VREF_MV) / PORT_ADC_MAX_LSB;

    int16_t temp_deci = 0;
    bsp_status_t ret = port_adc_read_temperature(&temp_deci);

    log_i("MCU Temp: samples=%u raw_avg=%lu vsense=%lu mV", (unsigned int)valid,
          (unsigned long)raw_avg, (unsigned long)vsense_mv);

    if (ret != BSP_OK)
    {
        log_e("temperature convert failed! ret = %d", ret);
        return -1;
    }

    /* 拆分整数与小数位打印，避免浮点格式化引入的库开销 */
    log_i("MCU Temperature = %d.%d C", (int)(temp_deci / 10), (int)(temp_deci % 10));

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化片内温度采集演示模块
 */
bsp_status_t app_mcu_temp_demo_init(void)
{
    bsp_status_t ret = port_adc_init();
    if (ret != BSP_OK)
    {
        log_e("port_adc init failed! ret = %d", ret);
        return ret;
    }

    log_i("MCU Temp Demo loaded. Try: mcu_temp [samples]");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, mcu_temp, shell_mcu_temp, Read MCU internal temperature with averaging);
