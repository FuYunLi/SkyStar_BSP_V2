#include "shell.h"
#include "bsp_uart.h"

Shell shell; // 对外暴露以便其他模块调用重绘 API
static char shell_buffer[512];

/**
 * @brief 移植层的底层写接口
 * @note 采用状态驱动与写入保障机制。由 bsp_uart_write 异步接管发送，成功则返回期望长度，失败则返回 0。
 */
static short user_shell_write(char *data, unsigned short len)
{
    if (bsp_uart_write((const uint8_t *)data, len) == BSP_OK)
    {
        return len;
    }
    return 0;
}

/**
 * @brief 移植层的底层读接口
 * @note 采用流量驱动与动态拉取机制。由 bsp_uart_read 返回实际读到的字节数，确保非阻塞即时消费。
 */
static short bsp_shell_read(char *data, unsigned short len)
{
    return (short)bsp_uart_read((uint8_t *)data, len);
}

/**
 * @brief 初始化板级 Shell 服务层
 * @return bsp_status_t 初始化状态，成功返回 BSP_OK
 */
bsp_status_t bsp_shell_init(void)
{
    shell.write = user_shell_write;
    shell.read  = bsp_shell_read; 
    shellInit(&shell, shell_buffer, sizeof(shell_buffer));
    return BSP_OK;
}

/**
 * @brief Shell 核心解析任务
 * @note 需在主循环或 RTOS 任务中周期性轮询调用
 */
void bsp_shell_process(void)
{
    shellTask(&shell);
}