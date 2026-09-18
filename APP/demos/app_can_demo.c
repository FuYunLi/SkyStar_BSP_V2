/**
 * @file app_can_demo.c
 * @brief 隔离 CAN 自检演示实现
 * @note 对标官方出厂 CAN 环回例程。can_selftest 在环回模式下完成
 *       "发送→接收→比对"闭环，无需对端设备与终端电阻即可验证
 *       MCU 侧整条链路（真实总线通联还需 120Ω 终端与对端节点）。
 *       【未验证】总线尚未实测。
 */

#define LOG_TAG "APP_CAN"

#include "app_can_demo.h"
#include "bsp_logger.h"
#include "port_can.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有变量
 * ================================================================ */

static bool s_can_started;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 解析 CAN ID 参数（支持 0x 前缀十六进制）
 */
static uint32_t s_parse_id(const char *str)
{
    if ((str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X')))
    {
        return (uint32_t)strtoul(str, NULL, 16);
    }

    return (uint32_t)strtoul(str, NULL, 10);
}

/**
 * @brief can_init Shell 指令入口：启动 CAN 接口
 */
static int shell_can_init(int argc, char *argv[])
{
    bool loopback = false;

    if (argc == 2)
    {
        loopback = (atoi(argv[1]) != 0);
    }

    bsp_status_t ret = port_can_init(PORT_CAN_1, loopback);
    if (ret != BSP_OK)
    {
        log_e("can init failed! ret = %d", ret);
        return -1;
    }

    s_can_started = true;
    log_i("CAN started (%s, 500kbps).", loopback ? "loopback" : "normal");
    return 0;
}

/**
 * @brief can_send Shell 指令入口：发送标准帧
 * @note 用法：can_send <id> <d0> [d1 ... d7]（十进制或 0x 十六进制）
 */
static int shell_can_send(int argc, char *argv[])
{
    if (!s_can_started)
    {
        log_e("CAN not started, run can_init first.");
        return -1;
    }

    if (argc < 3)
    {
        log_i("usage: can_send <id> <d0> [d1 ... d7]");
        return -1;
    }

    port_can_frame_t frame = {0};
    frame.id = s_parse_id(argv[1]);
    frame.dlc = (uint8_t)(argc - 2);

    if (frame.dlc > 8U)
    {
        frame.dlc = 8U;
    }

    for (uint32_t i = 0U; i < frame.dlc; i++)
    {
        frame.data[i] = (uint8_t)s_parse_id(argv[2 + i]);
    }

    bsp_status_t ret = port_can_send(PORT_CAN_1, &frame);
    if (ret != BSP_OK)
    {
        log_e("send failed! ret = %d", ret);
        return -1;
    }

    log_i("tx id=0x%03lX dlc=%u", (unsigned long)frame.id, (unsigned int)frame.dlc);
    return 0;
}

/**
 * @brief can_recv Shell 指令入口：轮询接收一帧
 */
static int shell_can_recv(int argc, char *argv[])
{
    if (!s_can_started)
    {
        log_e("CAN not started, run can_init first.");
        return -1;
    }

    (void)argc;
    (void)argv;

    port_can_frame_t frame = {0};
    bsp_status_t ret = port_can_poll_recv(PORT_CAN_1, &frame);
    if (ret != BSP_OK)
    {
        log_w("rx fifo empty.");
        return -1;
    }

    log_i("rx id=0x%03lX dlc=%u data=%02X %02X %02X %02X %02X %02X %02X %02X",
          (unsigned long)frame.id, (unsigned int)frame.dlc,
          frame.data[0], frame.data[1], frame.data[2], frame.data[3],
          frame.data[4], frame.data[5], frame.data[6], frame.data[7]);
    return 0;
}

/**
 * @brief can_selftest Shell 指令入口：环回模式收发闭环
 */
static int shell_can_selftest(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    port_can_frame_t tx_frame = {0};
    port_can_frame_t rx_frame = {0};

    tx_frame.id = 0x123U;
    tx_frame.dlc = 4U;
    tx_frame.data[0] = 0xDEU;
    tx_frame.data[1] = 0xADU;
    tx_frame.data[2] = 0xBEU;
    tx_frame.data[3] = 0xEFU;

    /* 环回模式初始化并自检，结束后关闭接口 */
    bsp_status_t ret = port_can_init(PORT_CAN_1, true);
    if (ret != BSP_OK)
    {
        log_e("loopback init failed! ret = %d", ret);
        return -1;
    }
    s_can_started = true;

    (void)port_can_send(PORT_CAN_1, &tx_frame);
    ret = port_can_poll_recv(PORT_CAN_1, &rx_frame);

    (void)port_can_deinit(PORT_CAN_1);
    s_can_started = false;

    if (ret != BSP_OK)
    {
        log_e("selftest FAIL: no frame received.");
        return -1;
    }

    if ((rx_frame.id != tx_frame.id) ||
        (rx_frame.dlc != tx_frame.dlc) ||
        (rx_frame.data[0] != tx_frame.data[0]) ||
        (rx_frame.data[3] != tx_frame.data[3]))
    {
        log_e("selftest FAIL: frame mismatch.");
        return -1;
    }

    log_i("selftest PASS: loopback frame verified.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 CAN 演示模块
 */
bsp_status_t app_can_demo_init(void)
{
    log_i("CAN Demo loaded. Try: can_selftest / can_init <loopback> / can_send / can_recv");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, can_init, shell_can_init, Start CAN (arg 1 = loopback mode));
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, can_send, shell_can_send, Send standard frame);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, can_recv, shell_can_recv, Poll receive one frame);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, can_selftest, shell_can_selftest, Run loopback selftest);
