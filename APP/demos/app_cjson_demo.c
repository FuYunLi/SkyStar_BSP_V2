/**
 * @file app_cjson_demo.c
 * @brief cJSON 协议控制自检演示实现
 * @note 对标 RocketPi 09_rocketpi_uart_control_led_cjson。协议约定：
 *       {"led":true}                核心板 LED 亮
 *       {"led":false}               核心板 LED 灭
 *       {"buzzer":false}            蜂鸣器关闭
 *       {"buzzer":{"freq":2000,"volume":50}}
 *                                   蜂鸣器以指定频率/音量发声
 *       执行结果以 JSON 回包输出（机器可读，故用 printf 保持纯净输出，
 *       不混入日志前缀，参见代码规范 9.2）。
 *
 *       cJSON 内存纪律：Parse/Print 产生的对象树全部堆分配，
 *       每条路径（成功/失败/错误）都必须以 cJSON_Delete/cJSON_free 收尾。
 */

#define LOG_TAG "APP_CJSON"

#include "app_cjson_demo.h"
#include "bsp_logger.h"
#include "dev_led.h"
#include "dev_buzzer.h"
#include "cJSON.h"
#include "shell.h"
#include <stdio.h>

/* ================================================================
 * 私有函数声明
 * ================================================================ */

static void s_apply_buzzer(const cJSON *item, uint16_t *freq_out, uint8_t *volume_out);
static void s_print_status(bool led_valid, bool led_on, bool buzzer_valid, uint16_t freq, uint8_t volume);

/* ================================================================
 * 私有函数实现
 * ================================================================ */

/**
 * @brief 应用 buzzer 字段：非对象一律关闭；对象则取 freq/volume 联动发声
 * @param item buzzer 字段对应的 cJSON 节点
 * @param[out] freq_out 回填实际生效的频率（未生效时回填 0）
 * @param[out] volume_out 回填实际生效的音量（未生效时回填 0）
 */
static void s_apply_buzzer(const cJSON *item, uint16_t *freq_out, uint8_t *volume_out)
{
    *freq_out = 0U;
    *volume_out = 0U;

    if (!cJSON_IsObject(item))
    {
        (void)dev_buzzer_off();
        return;
    }

    const cJSON *freq = cJSON_GetObjectItemCaseSensitive(item, "freq");
    const cJSON *volume = cJSON_GetObjectItemCaseSensitive(item, "volume");

    /* 数值字段类型校验 + 越界兜底，脏协议输入不允许透传到底层 */
    if (!cJSON_IsNumber(freq) || !cJSON_IsNumber(volume) ||
        (freq->valuedouble <= 0.0) || (freq->valuedouble > 10000.0) ||
        (volume->valuedouble < 0.0) || (volume->valuedouble > 100.0))
    {
        (void)dev_buzzer_off();
        return;
    }

    uint16_t freq_val = (uint16_t)freq->valuedouble;
    uint8_t vol_val = (uint8_t)volume->valuedouble;

    (void)dev_buzzer_tone(freq_val, vol_val);

    *freq_out = freq_val;
    *volume_out = vol_val;
}

/**
 * @brief 以 JSON 形式回包当前执行结果
 * @note 回包是机器可读的纯文本输出，按规范使用 printf 而非 log_x
 */
static void s_print_status(bool led_valid, bool led_on, bool buzzer_valid, uint16_t freq, uint8_t volume)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        printf("{\"error\":\"json alloc failed\"}\r\n");
        return;
    }

    if (led_valid)
    {
        (void)cJSON_AddBoolToObject(root, "led", led_on);
    }

    if (buzzer_valid)
    {
        cJSON *buzzer = cJSON_CreateObject();
        if (buzzer != NULL)
        {
            (void)cJSON_AddNumberToObject(buzzer, "freq", (double)freq);
            (void)cJSON_AddNumberToObject(buzzer, "volume", (double)volume);
            (void)cJSON_AddItemToObject(root, "buzzer", buzzer);
        }
    }

    char *rendered = cJSON_PrintUnformatted(root);
    if (rendered != NULL)
    {
        printf("%s\r\n", rendered);
        cJSON_free(rendered);
    }

    cJSON_Delete(root);
}

/**
 * @brief cjson_ctrl Shell 指令入口：解析并执行 JSON 控制命令
 */
static int shell_cjson_ctrl(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("usage: cjson_ctrl {\"led\":true} | {\"buzzer\":{\"freq\":2000,\"volume\":50}}\r\n");
        return -1;
    }

    /* 1. 解析：失败时可通过 cJSON_GetErrorPtr 定位出错位置 */
    cJSON *root = cJSON_Parse(argv[1]);
    if (root == NULL)
    {
        const char *err = cJSON_GetErrorPtr();
        printf("{\"error\":\"json parse failed\",\"at\":\"%s\"}\r\n", (err != NULL) ? err : "?");
        return -1;
    }

    /* 2. 提取与执行：led/buzzer 字段互相独立，缺省即跳过 */
    const cJSON *led = cJSON_GetObjectItemCaseSensitive(root, "led");
    const cJSON *buzzer = cJSON_GetObjectItemCaseSensitive(root, "buzzer");

    bool led_valid = cJSON_IsBool(led);
    bool buzzer_valid = (buzzer != NULL);
    bool led_on = false;
    uint16_t freq = 0U;
    uint8_t volume = 0U;

    if (led_valid)
    {
        led_on = cJSON_IsTrue(led);
        (void)dev_led_set(LED_CORE, led_on ? DEV_LED_ON : DEV_LED_OFF);
    }

    if (buzzer_valid)
    {
        s_apply_buzzer(buzzer, &freq, &volume);
    }

    /* 3. 回包与内存释放 */
    s_print_status(led_valid, led_on, buzzer_valid, freq, volume);
    cJSON_Delete(root);

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 cJSON 协议控制演示模块
 */
bsp_status_t app_cjson_demo_init(void)
{
    log_i("cJSON Demo loaded. Try: cjson_ctrl {\"led\":true}");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, cjson_ctrl, shell_cjson_ctrl, Execute JSON device control command);
