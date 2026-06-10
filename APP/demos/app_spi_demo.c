/**
 * @file app_spi_demo.c
 * @brief SPI 演示与测试模块源文件
 */

#include "app_spi_demo.h"
#include "port_spi.h"
#include "shell.h"
#define LOG_TAG "SPI_DEMO"
#include "elog.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static volatile bool         s_dma_done   = false;
static volatile bsp_status_t s_dma_result = BSP_OK;

/* DMA 传输完成回调 */
static void spi_dma_callback(uint8_t id, bsp_status_t result, void *user_ctx)
{
    s_dma_done   = true;
    s_dma_result = result;
}

/* ================================================================
 * Shell 命令实现
 * ================================================================ */

/* spi_loopback 指令实现 */
int shell_spi_loopback(int argc, char *argv[])
{
    if (argc < 2)
    {
        log_e("Usage: spi_loopback <1|2>");
        return -1;
    }

    int port = atoi(argv[1]);
    if (port != 1 && port != 2)
    {
        log_e("Invalid port: %d. Use 1 or 2.", port);
        return -2;
    }

    if (port == 1)
    {
        /* SPI1 测试：单向 DMA 发送验证 */
        log_i("--- SPI1 (Transmit Only) DMA Test ---");
        log_i("Initializing SPI1...");
        port_spi_init(PORT_SPI_1);

        static uint8_t tx_buf[64];
        for (int i = 0; i < 64; i++)
        {
            tx_buf[i] = (uint8_t)i;
        }

        s_dma_done   = false;
        s_dma_result = BSP_OK;

        log_i("Starting DMA transmit (64 bytes)...");
        bsp_status_t status = port_spi_write_dma(PORT_SPI_1, tx_buf, 64, spi_dma_callback, NULL);
        if (status != BSP_OK)
        {
            log_e("port_spi_write_dma failed, status: %d", (int)status);
            return -3;
        }

        // 阻塞等待 DMA 传输完毕（最长等待 100ms）
        status = port_spi_wait_complete(PORT_SPI_1, 100);
        if (status != BSP_OK)
        {
            log_e("port_spi_wait_complete timeout/error, status: %d", (int)status);
            return -4;
        }

        if (s_dma_done && s_dma_result == BSP_OK)
        {
            log_i("SPI1 DMA Transmit Test SUCCESS");
        }
        else
        {
            log_e("SPI1 DMA Transmit Callback reports error: %d", (int)s_dma_result);
        }
    }
    else if (port == 2)
    {
        /* SPI2 测试：双向自环校验 */
        log_i("--- SPI2 Full-Duplex Polling Loopback Test ---");
        log_i("Please ensure PC2 (MISO) and PC3 (MOSI) are shorted!");
        log_i("Initializing SPI2...");
        port_spi_init(PORT_SPI_2);

        uint8_t tx_buf[32];
        uint8_t rx_buf[32];
        for (int i = 0; i < 32; i++)
        {
            tx_buf[i] = (uint8_t)(i + 0xA0);
        }
        memset(rx_buf, 0, sizeof(rx_buf));

        log_i("Transmitting & Receiving 32 bytes...");
        bsp_status_t status = port_spi_write_read(PORT_SPI_2, tx_buf, rx_buf, 32, 100);
        if (status != BSP_OK)
        {
            log_e("port_spi_write_read failed, status: %d", (int)status);
            return -5;
        }

        // 比对发送与接收的数据是否完全相同
        if (memcmp(tx_buf, rx_buf, 32) == 0)
        {
            log_i("SPI2 Polling Loopback Test: [PASS]");
        }
        else
        {
            log_e("SPI2 Polling Loopback Test: [FAILED] Data mismatch!");
            log_e("TX: 0x%02X 0x%02X 0x%02X ...", tx_buf[0], tx_buf[1], tx_buf[2]);
            log_e("RX: 0x%02X 0x%02X 0x%02X ...", rx_buf[0], rx_buf[1], rx_buf[2]);
            return -6;
        }
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, spi_loopback, shell_spi_loopback, "SPI1 (DMA Transmit) / SPI2 (Polling Loopback) test");

/* spi_write 指令实现 */
int shell_spi_write(int argc, char *argv[])
{
    if (argc < 3)
    {
        log_e("Usage: spi_write <1|2> <string>");
        return -1;
    }

    int port = atoi(argv[1]);
    if (port != 1 && port != 2)
    {
        log_e("Invalid port: %d. Use 1 or 2.", port);
        return -2;
    }

    port_spi_id_t spi_id = (port == 1) ? PORT_SPI_1 : PORT_SPI_2;
    const char *payload  = argv[2];
    uint16_t   len       = (uint16_t)strlen(payload);

    log_i("SPI%d write payload: \"%s\" (%d bytes)", port, payload, len);
    port_spi_init(spi_id);

    bsp_status_t status = port_spi_write(spi_id, (const uint8_t *)payload, len, 100);
    if (status == BSP_OK)
    {
        log_i("SPI%d write success", port);
    }
    else
    {
        log_e("SPI%d write failed, status: %d", port, (int)status);
        return -3;
    }

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, spi_write, shell_spi_write, "Write string data to SPI1 or SPI2");

/* ================================================================
 * 公开接口 API
 * ================================================================ */

/**
 * @brief 初始化 SPI 演示与测试模块
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t app_spi_demo_init(void)
{
    log_i("[APP] SPI demo module initialized successfully");
    return BSP_OK;
}
