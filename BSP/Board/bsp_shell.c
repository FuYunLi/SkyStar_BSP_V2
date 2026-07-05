/**
 * @file bsp_shell.c
 * @brief 板级 Shell 服务层源文件
 * @note 将 Letter Shell 终端数据流重定向并挂载至 bsp_uart 服务层。
 */

#include "shell.h"
#include "bsp_uart.h"

/* Shell 实例，允许其他模块调用重绘 API */
Shell shell;
static char shell_buffer[512];

/**
 * @brief 移植层的底层写接口
 * @note 采用分包与写等待机制，当发送环形缓冲区满时进行有限等待，避免命令输出被截断。
 */
static short user_shell_write(char *data, unsigned short len)
{
    unsigned short remaining = len;
    uint32_t start_tick = HAL_GetTick();

    while (remaining > 0)
    {
        uint16_t free_space = bsp_uart_get_tx_free_space();
        if (free_space > 0)
        {
            uint16_t write_len = (remaining < (unsigned short)free_space) ? remaining : (unsigned short)free_space;
            if (bsp_uart_write((const uint8_t *)data, write_len) == BSP_OK)
            {
                data += write_len;
                remaining -= write_len;
                start_tick = HAL_GetTick(); // 重置超时计时器
            }
        }
        else
        {
            // 超过 500 毫秒未释放空间则强制退出，防止硬件挂死导致死锁
            if (HAL_GetTick() - start_tick > 500U)
            {
                break;
            }
        }
    }

    return len - remaining;
}

/**
 * @brief 移植层的底层读接口
 * @note 采用流量驱动与动态拉取机制。由 bsp_uart_read 返回实际读到的字节数，确保非阻塞即时消费。
 */
static short bsp_shell_read(char *data, unsigned short len)
{
    extern volatile bool g_ymodem_active;
    
    if (g_ymodem_active)
    {
        return 0;
    }
    
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