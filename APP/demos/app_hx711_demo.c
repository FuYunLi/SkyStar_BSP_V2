/**
 * @file app_hx711_demo.c
 * @brief HX711 称重采集自检演示实现
 * @note 对标官方出厂 hx711_driver.c。校准流程：空载时 hx711_tare
 *       记录皮重，再挂已知重量并以 hx711_scale 写入刻度系数。
 *       【未验证】外部称重传感器尚未接线实测。
 */

#define LOG_TAG "APP_HX711"

#include "app_hx711_demo.h"
#include "bsp_logger.h"
#include "dev_hx711.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief hx711_raw Shell 指令入口：读取原始值
 */
static int shell_hx711_raw(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int32_t raw = 0;
    bsp_status_t ret = dev_hx711_read_raw(&raw);
    if (ret != BSP_OK)
    {
        log_e("read raw failed! ret = %d", ret);
        return -1;
    }

    log_i("HX711 raw = %ld", (long)raw);
    return 0;
}

/**
 * @brief hx711_tare Shell 指令入口：空载去皮
 */
static int shell_hx711_tare(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    int32_t raw = 0;
    bsp_status_t ret = dev_hx711_read_raw(&raw);
    if (ret != BSP_OK)
    {
        log_e("tare failed! ret = %d", ret);
        return -1;
    }

    (void)dev_hx711_set_offset(raw);
    log_i("tare offset = %ld", (long)raw);
    return 0;
}

/**
 * @brief hx711_scale Shell 指令入口：设置刻度系数
 */
static int shell_hx711_scale(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: hx711_scale <counts_per_gram>");
        return -1;
    }

    float scale = (float)atof(argv[1]);

    bsp_status_t ret = dev_hx711_set_scale(scale);
    if (ret != BSP_OK)
    {
        log_e("set scale failed!");
        return -1;
    }

    log_i("scale = %s counts/gram", argv[1]);
    return 0;
}

/**
 * @brief hx711_weight Shell 指令入口：读取净重
 */
static int shell_hx711_weight(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    float weight = 0.0f;
    bsp_status_t ret = dev_hx711_read_weight_g(&weight);
    if (ret != BSP_OK)
    {
        log_e("read weight failed! ret = %d", ret);
        return -1;
    }

    log_i("weight = %d.%02d g", (int)weight, (int)((weight < 0 ? -weight : weight) * 100) % 100);
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 HX711 称重演示模块
 */
bsp_status_t app_hx711_demo_init(void)
{
    bsp_status_t ret = dev_hx711_init();
    if (ret != BSP_OK)
    {
        return ret;
    }

    log_i("HX711 Demo loaded. Try: hx711_raw / hx711_tare / hx711_scale / hx711_weight");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, hx711_raw, shell_hx711_raw, Read HX711 raw 24bit value);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, hx711_tare, shell_hx711_tare, Zero the scale (tare));
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, hx711_scale, shell_hx711_scale, Set scale factor (counts per gram));
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, hx711_weight, shell_hx711_weight, Read weight in grams);
