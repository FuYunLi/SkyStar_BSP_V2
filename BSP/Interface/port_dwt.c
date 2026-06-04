/**
 * @file port_dwt.c
 * @brief DWT 周期计数器接口实现
 * @note 基于 ARM Cortex-M DWT CYCCNT 寄存器，提供 CPU 周期级计时
 *       参考：ARM CoreSight Technical Reference Manual, DWT 章节
 */

#include "port_dwt.h"
#include <stdint.h>

/* 初始化状态：0=未初始化, 1=成功, 2=失败 */
static uint8_t dwt_status = 0;

/**
 * @brief 初始化 DWT 周期计数器
 * @note 仅首次调用生效，重复调用安全返回
 *       原理：使能 CoreDebug TRCENA 位以开启跟踪单元，
 *       再置位 DWT CYCCNTENA 使能自由运行的周期计数器
 */
bsp_status_t port_dwt_init(void)
{
    if (dwt_status != 0)
    {
        return (dwt_status == 1) ? BSP_OK : BSP_ERROR;
    }

    /* 使能跟踪单元（TRC），DWT 属于跟踪子系统 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

#if defined(DWT_LAR)
    /* Cortex-M7 等内核的 DWT 需要先解锁才能写入控制寄存器 */
    DWT->LAR = 0xC5ACCE55;
#endif

    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 回读验证：确认 CYCCNTENA 位确实被置位 */
    dwt_status = (DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) ? 1 : 2;

    return (dwt_status == 1) ? BSP_OK : BSP_ERROR;
}

/**
 * @brief 获取当前 DWT 周期计数值
 * @return 32位自由运行计数器值（溢出后自动回绕）
 */
uint32_t port_dwt_get_cycles(void)
{
    return DWT->CYCCNT;
}

/**
 * @brief 基于 DWT 的精确微秒阻塞延时
 * @param us 延时微秒数（建议 ≤ 25000000，避免周期数溢出）
 * @note 原理：DWT->CYCCNT 以 SystemCoreClock 频率自由递增，
 *       无符号差值比较天然处理 32 位溢出回绕（168MHz 下约 25.5s）
 *
 * @code
 * port_dwt_delay_us(5);  // 精确延时 5μs，适用于软件 I2C 时序
 * @endcode
 */
void port_dwt_delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000u) * us;
    uint32_t start  = DWT->CYCCNT;
    while (DWT->CYCCNT - start < cycles);
}