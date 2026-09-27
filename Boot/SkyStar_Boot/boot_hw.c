/**
 * @file boot_hw.c
 * @brief Bootloader 基础硬件服务实现
 * @note 寄存器级配置对照 RM0090：时钟树 /M=4 ×N=168 /P=2 /Q=7
 *       （Q=7 兼得 USB 48MHz 与 SDIO 48MHz），Flash 5WS 等待周期。
 */

#include "boot_hw.h"
#include "stm32f407xx.h"

/* ================================================================
 * 私有变量
 * ================================================================ */

static volatile uint32_t s_tick_ms;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief SysTick 中断（1ms 时基，优先级最低）
 */
void SysTick_Handler(void)
{
    s_tick_ms++;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 时钟树初始化：HSE 8MHz → PLL → SYSCLK 168MHz
 */
void boot_hw_clock_init(void)
{
    /* 1. HSE 启动并等待就绪 */
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0U)
    {
    }

    /* 2. 电压调节器 Scale1（168MHz 前置条件，VOS[15:14] = 11） */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= (3UL << 14);

    /* 3. Flash 等待周期 5WS + 预取 + 指令/数据缓存 */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* 4. 总线分频：AHB=168MHz，APB1=42MHz，APB2=84MHz */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* 5. 主 PLL：/M=4 ×N=168 /P=2（168MHz） /Q=7（48MHz） */
    RCC->PLLCFGR = 4U | (168U << 6U) | RCC_PLLCFGR_PLLSRC_HSE | (7U << 24U);
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    /* 6. 切换系统时钟至 PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    SystemCoreClock = 168000000U;
}

/**
 * @brief 调试串口初始化：PA9=TX PA10=RX（AF7），115200-8-N-1
 */
void boot_hw_uart_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PA9/PA10 复用推挽 AF7 */
    GPIOA->MODER &= ~((3UL << 18) | (3UL << 20));
    GPIOA->MODER |= (2UL << 18) | (2UL << 20);
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((0xFUL << 4) | (0xFUL << 8))) |
                    (7UL << 4) | (7UL << 8);
    GPIOA->OSPEEDR |= (3UL << 18) | (3UL << 20);

    /* APB2=84MHz：BRR = 84e6/16/115200 ≈ 45.5625 → 0x2D9 */
    USART1->BRR = 0x2D9U;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/**
 * @brief 轮询打印字符串
 */
void boot_hw_print(const char *str)
{
    while (*str != '\0')
    {
        while ((USART1->SR & USART_SR_TXE) == 0U)
        {
        }
        USART1->DR = (uint16_t)*str++;
    }

    /* 等待移位器排空，保证日志完整后才能切走外设 */
    while ((USART1->SR & USART_SR_TC) == 0U)
    {
    }
}

/**
 * @brief 轮询打印十六进制数值
 */
void boot_hw_print_hex(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char buf[11];
    int32_t i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; i++)
    {
        buf[2 + i] = digits[(value >> (28 - 4 * i)) & 0xFU];
    }
    buf[10] = '\0';

    boot_hw_print(buf);
}

/**
 * @brief SysTick 启动 1ms 时基
 */
static void s_boot_hw_systick_init(void)
{
    s_tick_ms = 0U;
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

/**
 * @brief 毫秒级阻塞延时
 */
void boot_hw_delay_ms(uint32_t ms)
{
    uint32_t start = s_tick_ms;

    while ((s_tick_ms - start) < ms)
    {
    }
}

/**
 * @brief 毫秒时基查询
 */
uint32_t boot_hw_tick_ms(void)
{
    return s_tick_ms;
}

/**
 * @brief Bootloader 侧外设总初始化入口
 */
void boot_hw_init(void)
{
    boot_hw_clock_init();
    s_boot_hw_systick_init();
    boot_hw_uart_init();
}
