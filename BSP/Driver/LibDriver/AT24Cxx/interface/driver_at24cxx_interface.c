/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * 
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 *
 * @file      driver_at24cxx_interface.c
 * @brief     driver at24cxx interface source file
 * @version   2.0.0
 * @author    Shifeng Li
 * @date      2021-02-17
 */

#include "driver_at24cxx_interface.h"
#include "port_i2c.h"
#include "port_dwt.h"
#include <stdarg.h>
#include <stdio.h>

/**
 * @brief  interface iic bus init
 * @return status code
 *         - 0 success
 *         - 1 iic init failed
 * @note   none
 */
uint8_t at24cxx_interface_iic_init(void)
{
    bsp_status_t status = port_i2c_init(PORT_I2C_1);
    if (status == BSP_OK)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief  interface iic bus deinit
 * @return status code
 *         - 0 success
 *         - 1 iic deinit failed
 * @note   none
 */
uint8_t at24cxx_interface_iic_deinit(void)
{
    return 0;
}

/**
 * @brief      interface iic bus read
 * @param[in]  addr iic device write address
 * @param[in]  reg iic register address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t at24cxx_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    bsp_status_t status = port_i2c_mem_read(PORT_I2C_1, addr, reg, 1, buf, len, 1000);
    if (status == BSP_OK)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief     interface iic bus write
 * @param[in] addr iic device write address
 * @param[in] reg iic register address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t at24cxx_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    bsp_status_t status = port_i2c_mem_write(PORT_I2C_1, addr, reg, 1, buf, len, 1000);
    if (status == BSP_OK)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief      interface iic bus read with 16 bits register address
 * @param[in]  addr iic device write address
 * @param[in]  reg iic register address
 * @param[out] *buf pointer to a data buffer
 * @param[in]  len length of the data buffer
 * @return     status code
 *             - 0 success
 *             - 1 read failed
 * @note       none
 */
uint8_t at24cxx_interface_iic_read_address16(uint8_t addr, uint16_t reg, uint8_t *buf, uint16_t len)
{
    bsp_status_t status = port_i2c_mem_read(PORT_I2C_1, addr, reg, 2, buf, len, 1000);
    if (status == BSP_OK)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief     interface iic bus write with 16 bits register address
 * @param[in] addr iic device write address
 * @param[in] reg iic register address
 * @param[in] *buf pointer to a data buffer
 * @param[in] len length of the data buffer
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
uint8_t at24cxx_interface_iic_write_address16(uint8_t addr, uint16_t reg, uint8_t *buf, uint16_t len)
{
    bsp_status_t status = port_i2c_mem_write(PORT_I2C_1, addr, reg, 2, buf, len, 1000);
    if (status == BSP_OK)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

/**
 * @brief     interface delay ms
 * @param[in] ms time
 * @note      none
 */
void at24cxx_interface_delay_ms(uint32_t ms)
{
    port_dwt_delay_us(ms * 1000);
}

/**
 * @brief     interface print format data
 * @param[in] fmt format data
 * @note      none
 */
void at24cxx_interface_debug_print(const char *const fmt, ...)
{
    char str[256];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);
    printf("%s", str);
}
