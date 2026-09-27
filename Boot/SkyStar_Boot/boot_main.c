/**
 * @file boot_main.c
 * @brief SkyStar Bootloader 主入口
 * @note 职责单一：初始化最小硬件 → 执行 OTA 流水线（含跳转）。
 *       链接于 0x08000000/32KB，不依赖主工程 BSP 与 HAL。
 */

#include "boot_hw.h"
#include "boot_w25q.h"
#include "boot_ota.h"

/* Bootloader 版本：OTA 传输侧的 version 字段与之比对可判断升降级 */
#define BOOT_VERSION (0x00010000UL) /* v1.0.0 */

/**
 * @brief Bootloader 主函数
 * @note 正常路径由 boot_ota_process 内部跳转进 App，不会返回
 */
int main(void)
{
    boot_hw_init();

    boot_hw_print("\r\n=== SkyStar Bootloader v1.0 ===\r\n");
    boot_hw_print("clock: 168MHz, uart1: 115200\r\n");

    if (!boot_w25q_init())
    {
        boot_hw_print("W25Q init FAIL, jumping app anyway.\r\n");
        boot_ota_process(); /* 内部会因读不到有效头部而直接跳转 */
    }

    boot_ota_process();

    /* 兜底：跳转失败（App 无效）时停在死循环等待调试器 */
    boot_hw_print("halt: no valid app.\r\n");
    for (;;)
    {
    }
}
