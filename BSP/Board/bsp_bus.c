/**
 * @file bsp_bus.c
 * @brief 板级总线仲裁服务源文件
 */

#include "bsp_bus.h"
#include "dev_pca9555.h"
#include "port_spi.h"
#include "port_i2s.h"
#include "bsp_imu.h"
#include "bsp_logger.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BSP_BUS"

/* 模拟开关引脚配置：Port 0 Pin 2 */
#define SWITCH_PCA_PORT  (0U)
#define SWITCH_PCA_PIN   (2U)

/* 外部 PCA9555 全局唯一芯片实例 (定义在 bsp_led.c 中) */
extern dev_pca9555_t g_pca_led;

/* 总线当前的工作状态 */
static bsp_bus_mode_t s_bus_mode = BSP_BUS_MODE_SPI;

/**
 * @brief  初始化总线仲裁器
 */
bsp_status_t bsp_bus_init(void)
{
    /* 1. 配置 PCA9555 Port 0 Pin 2 为输出 */
    bsp_status_t status = dev_pca9555_set_pin_dir(&g_pca_led, SWITCH_PCA_PORT, SWITCH_PCA_PIN, 0);
    if (status != BSP_OK)
    {
        log_e("Failed to config PCA9555 switch pin dir");
        return status;
    }

    /* 2. 默认选择 SPI 模式，拉低 PIN2 */
    status = dev_pca9555_write_pin(&g_pca_led, SWITCH_PCA_PORT, SWITCH_PCA_PIN, DEV_PCA9555_RESET);
    if (status != BSP_OK)
    {
        log_e("Failed to select default SPI bus via PCA9555");
        return status;
    }

    s_bus_mode = BSP_BUS_MODE_SPI;
    log_i("Bus arbiter initialized, default SPI mode active");
    return BSP_OK;
}

/**
 * @brief  申请占有指定总线通道并切换到指定模式
 */
bsp_status_t bsp_bus_acquire(bsp_bus_id_t id, bsp_bus_mode_t mode)
{
    if (id >= BSP_BUS_MAX)
    {
        return BSP_EINVAL;
    }

    if (s_bus_mode == mode)
    {
        return BSP_OK;
    }

    log_i("Switching bus mode: %s -> %s", 
          (s_bus_mode == BSP_BUS_MODE_SPI) ? "SPI" : "I2S",
          (mode == BSP_BUS_MODE_SPI) ? "SPI" : "I2S");

    if (mode == BSP_BUS_MODE_I2S)
    {
        /* A. 挂起 IMU 周期读取，防止读取物理总线导致总线冲突 */
        bsp_imu_suspend();

        /* B. 反初始化 SPI2 (释放 SPI2 相关的管脚复用，防止引脚电平冲突) */
        port_spi_deinit(PORT_SPI_2);

        /* C. 控制 PCA9555 模拟开关选通 I2S2 (拉高管脚) */
        bsp_status_t status = dev_pca9555_write_pin(&g_pca_led, SWITCH_PCA_PORT, SWITCH_PCA_PIN, DEV_PCA9555_SET);
        if (status != BSP_OK)
        {
            log_e("PCA9555 switch to I2S failed");
            return status;
        }

        /* D. 初始化 I2S2 驱动与 DMA */
        status = port_i2s_init();
        if (status != BSP_OK)
        {
            log_e("Port I2S init failed during bus acquisition");
            return status;
        }

        s_bus_mode = BSP_BUS_MODE_I2S;
    }
    else
    {
        /* A. 反初始化 I2S2，停止音频 DMA 传输 */
        port_i2s_deinit();

        /* B. 控制 PCA9555 模拟开关选通 SPI2 (拉低管脚) */
        bsp_status_t status = dev_pca9555_write_pin(&g_pca_led, SWITCH_PCA_PORT, SWITCH_PCA_PIN, DEV_PCA9555_RESET);
        if (status != BSP_OK)
        {
            log_e("PCA9555 switch to SPI failed");
            return status;
        }

        /* C. 重新调用 CubeMX 生成的 SPI2 初始化配置硬件并切换引脚到 SPI 复用模式 */
        extern void MX_SPI2_Init(void);
        MX_SPI2_Init();

        s_bus_mode = BSP_BUS_MODE_SPI;

        /* D. 重新初始化 BSP SPI2 软件上下文 */
        status = port_spi_init(PORT_SPI_2);
        if (status != BSP_OK)
        {
            s_bus_mode = BSP_BUS_MODE_I2S;
            log_e("Port SPI2 re-init failed during bus acquisition");
            return status;
        }

        /* E. 恢复 IMU 周期读取 */
        bsp_imu_resume();
    }

    log_i("Bus mode switch completed successfully");
    return BSP_OK;
}

/**
 * @brief  释放指定总线通道的占有（默认切回 SPI）
 */
bsp_status_t bsp_bus_release(bsp_bus_id_t id)
{
    return bsp_bus_acquire(id, BSP_BUS_MODE_SPI);
}

/**
 * @brief  获取指定总线通道的当前工作模式
 */
bsp_bus_mode_t bsp_bus_get_mode(bsp_bus_id_t id)
{
    (void)id;
    return s_bus_mode;
}
