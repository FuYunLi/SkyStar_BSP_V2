/*
 * This file is part of the EasyLogger Library.
 *
 * Copyright (c) 2015, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2015-04-28
 */

#include <elog.h>
#include <stdio.h>

#include "port_tick.h"
#include "bsp_uart.h"

/**
 * EasyLogger port initialize
 *
 * @return result
 */
ElogErrCode elog_port_init(void)
{
    ElogErrCode result = ELOG_NO_ERR;

    /* add your code here */

    return result;
}

/**
 * output log port interface
 *
 * @param log output of log
 * @param size log size
 */
void elog_port_output(const char *log, size_t size)
{
    size_t remaining = size;
    while (remaining > 0)
    {
        uint16_t free_space = bsp_uart_get_tx_free_space();
        if (free_space > 0)
        {
            uint16_t write_len = (remaining < (size_t)free_space) ? (uint16_t)remaining : free_space;
            if (bsp_uart_write((const uint8_t *)log, write_len) == BSP_OK)
            {
                log += write_len;
                remaining -= write_len;
            }
            else
            {
                if (__get_PRIMASK() != 0U || __get_IPSR() != 0U)
                {
                    break;
                }
            }
        }
        else
        {
            /* 
             * EasyLogger 会在 elog_port_output_lock 中关闭全局中断。
             * 此时 DMA 发送完成中断无法触发，发送队列无法腾出空间。
             * 因此如果队列已满，绝对不能死等，只能丢弃剩余数据防止死锁！
             */
            if (__get_PRIMASK() != 0U || __get_IPSR() != 0U)
            {
                break;
            }
        }
    }
}

/**
 * output lock
 */
void elog_port_output_lock(void)
{
    /* add your code here */
    __disable_irq();
}

/**
 * output unlock
 */
void elog_port_output_unlock(void)
{
    /* add your code here */
    __enable_irq();
}

/**
 * get current time interface
 *
 * @return current time
 */
const char *elog_port_get_time(void)
{
    /* add your code here */
    static char cur_system_time[16] = { 0 };

    /* 调用 SkyStar 的 Tick 接口获取毫秒数 */
    snprintf(cur_system_time, sizeof(cur_system_time), "%lu", port_tick_get_ms());

    return cur_system_time;
}

/**
 * get current process name interface
 *
 * @return current process name
 */
const char *elog_port_get_p_info(void)
{ /* add your code here */
    return "";
}

/**
 * get current thread name interface
 *
 * @return current thread name
 */
const char *elog_port_get_t_info(void)
{ /* add your code here */
    return "";
}
