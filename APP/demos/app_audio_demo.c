/**
 * @file app_audio_demo.c
 * @brief 自检演示模块——音频子系统 Shell 自检指令实现
 * @note  M30：导出 audio_bus_switch 命令，验证 SPI2/I2S2 复用总线的
 *        软件仲裁切换（挂起 IMU → 切模拟开关 → 外设模式重配）。
 */

#define LOG_TAG "APP_AUDIO"

#include "app_audio_demo.h"
#include "bsp_bus.h"
#include "bsp_logger.h"
#include "shell.h"
#include <stdio.h>
#include <string.h>

/* ================================================================
 * 私有函数声明与实现
 * ================================================================ */

/**
 * @brief 手动切换 SPI2/I2S2 复用总线归属
 * @note  用法：audio_bus_switch [i2s|spi]
 */
static void shell_audio_bus_switch(int argc, char *argv[])
{
    bsp_bus_owner_t target;
    if (argc >= 2 && strcmp(argv[1], "i2s") == 0)
    {
        target = BSP_BUS_OWNER_I2S2;
    }
    else if (argc >= 2 && strcmp(argv[1], "spi") == 0)
    {
        target = BSP_BUS_OWNER_SPI2;
    }
    else
    {
        printf("Usage: audio_bus_switch [i2s|spi]\r\n");
        printf("Current owner: %s\r\n",
               (bsp_bus_current(BSP_BUS_SPI2_I2S2) == BSP_BUS_OWNER_I2S2) ? "I2S2"
               : (bsp_bus_current(BSP_BUS_SPI2_I2S2) == BSP_BUS_OWNER_SPI2) ? "SPI2"
                                                                            : "NONE");
        return;
    }

    /* 手动切换属接管语义：先释放当前归属方的占用权再申请，
     * 切换链路内部会自动挂起 IMU / 停止 I2S 流，接管是安全的 */
    bsp_bus_owner_t current = bsp_bus_current(BSP_BUS_SPI2_I2S2);
    if (current != BSP_BUS_OWNER_NONE && current != target)
    {
        (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, current);
    }

    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, target);
    if (status != BSP_OK)
    {
        printf("Bus switch failed, ret = %d\r\n", status);
        return;
    }
    printf("Bus switched to %s OK\r\n", (target == BSP_BUS_OWNER_I2S2) ? "I2S2" : "SPI2");
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t app_audio_demo_init(void)
{
    log_i("Audio demo module registered (M30 bus arbitration)");
    return BSP_OK;
}

/* ================================================================
 * Shell 命令导出
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN,
                 audio_bus_switch, shell_audio_bus_switch, Switch SPI2/I2S2 shared bus [i2s|spi]);
