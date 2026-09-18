/**
 * @file app_rs485_demo.c
 * @brief 隔离 RS485 自检演示实现
 * @note 对标官方出厂 drv_rs485。【未验证】RS485 对端尚未接线实测。
 *       环回自测需在 A/B 端子外接短接或对端设备。
 */

#define LOG_TAG "APP_RS485"

#include "app_rs485_demo.h"
#include "bsp_logger.h"
#include "bsp_board.h"
#include "bsp_rs485.h"
#include "shell.h"
#include <string.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define RS485_DEMO_RECV_LEN     (32U)   /* 接收缓冲长度 */
#define RS485_DEMO_RECV_TIMEOUT (2000U) /* 接收超时（ms） */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief rs485_send Shell 指令入口：发送字符串
 */
static int shell_rs485_send(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: rs485_send <string>");
        return -1;
    }

    uint16_t len = (uint16_t)strlen(argv[1]);

    bsp_status_t ret = bsp_rs485_send((const uint8_t *)argv[1], len);
    if (ret != BSP_OK)
    {
        log_e("send failed! ret = %d", ret);
        return -1;
    }

    log_i("sent %u bytes.", (unsigned int)len);
    return 0;
}

/**
 * @brief rs485_recv Shell 指令入口：阻塞接收并回显
 */
static int shell_rs485_recv(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint8_t buf[RS485_DEMO_RECV_LEN + 1U];

    bsp_status_t ret = bsp_rs485_recv(buf, RS485_DEMO_RECV_LEN, RS485_DEMO_RECV_TIMEOUT);
    if (ret == BSP_ETIMEOUT)
    {
        log_w("recv timeout (%u ms).", (unsigned int)RS485_DEMO_RECV_TIMEOUT);
        return -1;
    }
    if (ret != BSP_OK)
    {
        log_e("recv failed! ret = %d", ret);
        return -1;
    }

    buf[RS485_DEMO_RECV_LEN] = 0U;
    log_i("recv: %s", (char *)buf);
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 RS485 演示模块
 */
bsp_status_t app_rs485_demo_init(void)
{
    bsp_status_t ret = bsp_rs485_init();
    if (ret != BSP_OK)
    {
        return ret;
    }

    log_i("RS485 Demo loaded. Try: rs485_send <str> / rs485_recv");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, rs485_send, shell_rs485_send, Send string over RS485);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, rs485_recv, shell_rs485_recv, Receive from RS485 (blocking with timeout));
