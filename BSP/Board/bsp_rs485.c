/**
 * @file bsp_rs485.c
 * @brief 隔离 RS485 总线服务层实现
 * @note 数据通道为 USART3（port_uart 自持初始化，轮询模式），
 *       方向控制为 PD15（RE/DE 复用脚，高发送低接收）。
 *       半双工纪律：同一时刻总线上只允许一方驱动。
 *       【未验证】RS485 对端尚未接线实测。
 */

#define LOG_TAG "BSP_RS485"

#include "bsp_rs485.h"
#include "bsp_logger.h"
#include "port_uart.h"
#include "port_gpio.h"

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 RS485 服务
 */
bsp_status_t bsp_rs485_init(void)
{
    port_uart_config_t cfg = {0};

    /* 轮询模式：通道内不使用 DMA/环形缓冲，cfg 其余字段忽略 */
    cfg.baudrate = 115200U;

    bsp_status_t ret = port_uart_init(PORT_UART_3, &cfg);
    if (ret != BSP_OK)
    {
        log_e("USART3 init failed! ret = %d", ret);
        return ret;
    }

    /* 默认进入接收态 */
    (void)port_gpio_write(PORT_GPIO_RS485_DE, PORT_GPIO_LOW);

    log_i("RS485 ready (115200-8-N-1).");
    return BSP_OK;
}

/**
 * @brief 发送一帧数据
 */
bsp_status_t bsp_rs485_send(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0))
    {
        return BSP_EINVAL;
    }

    /* 切换为发送态后提交数据；port_uart_write 阻塞至 TC（移位器排空），
     * 返回后立刻切回接收态是安全的 */
    (void)port_gpio_write(PORT_GPIO_RS485_DE, PORT_GPIO_HIGH);

    bsp_status_t ret = port_uart_write(PORT_UART_3, data, len);

    (void)port_gpio_write(PORT_GPIO_RS485_DE, PORT_GPIO_LOW);

    return ret;
}

/**
 * @brief 接收一帧数据
 */
bsp_status_t bsp_rs485_recv(uint8_t *buf, uint16_t len, uint32_t timeout_ms)
{
    if ((buf == NULL) || (len == 0))
    {
        return BSP_EINVAL;
    }

    return port_uart_read_poll(PORT_UART_3, buf, len, timeout_ms);
}
