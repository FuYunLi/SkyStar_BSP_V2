/**
 * @file app_crc_demo.c
 * @brief CRC 校验自检演示实现
 * @note 对标 RocketPi 40_rocketpi_crc。实现 CRC-32/ISO-HDLC（即 zip/
 *       Ethernet 同款参数）：多项式 0x04C11DB7，输入输出反射，
 *       初值 0xFFFFFFFF，终异或 0xFFFFFFFF——等价的反射实现多项式
 *       为 0xEDB88320，逐位计算无需 256 项查表。
 *
 *       权威测试向量：ASCII "123456789" 的 CRC-32 = 0xCBF43926，
 *       任何实现算不出此值即参数有误。
 *
 *       延伸方向：F407 片上有硬件 CRC 外设（固定多项式、不反射、
 *       MSB 先行），与通用软件 CRC-32 参数不同，算值不可直接互比；
 *       如需启用应新增 port_crc 接口模块承载寄存器访问。
 */

#define LOG_TAG "APP_CRC"

#include "app_crc_demo.h"
#include "bsp_logger.h"
#include "shell.h"
#include <string.h>

/* ================================================================
 * 私有宏定义与常量
 * ================================================================ */

#define CRC32_POLY_REFLECTED (0xEDB88320UL) /* 反射形式多项式 */
#define CRC32_CHECK_VALUE    (0xCBF43926UL) /* "123456789" 的标准答案 */

static const char s_crc_check_str[] = "123456789";

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 软件计算字节流的 CRC-32（逐位法）
 * @param data 数据指针
 * @param len  字节数
 * @return uint32_t CRC-32 校验值
 */
static uint32_t s_crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;

    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= (uint32_t)data[i];

        /* 每字节 8 轮： LSB 已异或进 crc，右移 1 位后按最低位决定
         * 是否异或反射多项式——等价于手算除法中的"商 1 则减" */
        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1UL) != 0UL)
            {
                crc = (crc >> 1) ^ CRC32_POLY_REFLECTED;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

/**
 * @brief crc_selftest Shell 指令入口：标准测试向量自检
 */
static int shell_crc_selftest(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t crc = s_crc32_calc((const uint8_t *)s_crc_check_str, (uint32_t)strlen(s_crc_check_str));

    if (crc == CRC32_CHECK_VALUE)
    {
        log_i("CRC32 selftest PASS: crc(\"%s\") = 0x%08lX", s_crc_check_str, (unsigned long)crc);
        return 0;
    }

    log_e("CRC32 selftest FAIL: got 0x%08lX, expect 0x%08lX", (unsigned long)crc,
          (unsigned long)CRC32_CHECK_VALUE);
    return -1;
}

/**
 * @brief crc_calc Shell 指令入口：计算任意字符串的 CRC-32
 */
static int shell_crc_calc(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: crc_calc <string>");
        return -1;
    }

    uint32_t crc = s_crc32_calc((const uint8_t *)argv[1], (uint32_t)strlen(argv[1]));
    log_i("crc32(\"%s\") = 0x%08lX", argv[1], (unsigned long)crc);

    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 CRC 校验演示模块
 */
bsp_status_t app_crc_demo_init(void)
{
    log_i("CRC Demo loaded. Try: crc_selftest / crc_calc <string>");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, crc_selftest, shell_crc_selftest, Run CRC32 standard test vector);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, crc_calc, shell_crc_calc, Compute CRC32 of a string);
