/**
 * @file bsp_bus.c
 * @brief 板级共享总线仲裁器实现
 * @note  SPI2/I2S2 共享 PB10/PC2/PC3 三条物理信号线，由三路模拟开关
 *        （U31/U33/U34）分时选通；开关控制网络经 PCA9555 Port0 Pin2
 *        （对应拨码开关 BIT3，拉低 = SPI2 侧导通，拉高 = I2S2 侧导通）
 *        软件控制，优先级高于拨码开关。切换遵循自底向上顺序：
 *        挂起上层使用方 → 反初始化旧模式外设 → 切换模拟开关 → 初始化新模式。
 */

#define LOG_TAG "BSP_BUS"

#include "bsp_bus.h"
#include "port_spi.h"
#include "port_i2s.h"
#include "dev_pca9555.h"
#include "port_critical.h"
#include "bsp_imu.h"
#include "bsp_logger.h"
#include "spi.h"

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* 模拟开关控制位：PCA9555 Port0 Pin2（与拨码开关 BIT3 对应，见 bsp_imu 原实现） */
#define BUS_SWITCH_PCA_PORT (0U)
#define BUS_SWITCH_PCA_PIN  (2U)

/* 模拟开关电平语义：低电平选通 SPI2 侧，高电平选通 I2S2 侧 */
#define BUS_SWITCH_LEVEL_SPI2 (DEV_PCA9555_RESET)
#define BUS_SWITCH_LEVEL_I2S2 (DEV_PCA9555_SET)

/* ================================================================
 * 外部实例声明
 * ================================================================ */

/* 全局唯一 PCA9555 物理芯片实例（定义于 bsp_led.c） */
extern dev_pca9555_t g_pca_led;

/* ================================================================
 * 私有变量
 * ================================================================ */

static bsp_bus_owner_t s_bus_owner[BSP_BUS_MAX] = {BSP_BUS_OWNER_NONE};
static bsp_bus_owner_t s_bus_physical[BSP_BUS_MAX] = {BSP_BUS_OWNER_SPI2};
static bool s_switch_inited = false;
/* 复位后模拟开关控制位须由软件显式驱动：PCA9555 复位默认高阻，
 * 控制网络无外部上下拉，浮空将导致总线随机选通（见原理图风险说明） */

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

/**
 * @brief 将模拟开关切换到指定物理侧
 */
static bsp_status_t bus_switch_physical(bsp_bus_owner_t target)
{
    dev_pca9555_state_t level = (target == BSP_BUS_OWNER_I2S2) ? BUS_SWITCH_LEVEL_I2S2 : BUS_SWITCH_LEVEL_SPI2;

    bsp_status_t status = dev_pca9555_set_pin_dir(&g_pca_led, BUS_SWITCH_PCA_PORT, BUS_SWITCH_PCA_PIN, 0);
    if (status != BSP_OK)
    {
        log_e("PCA9555 switch dir config failed");
        return status;
    }

    status = dev_pca9555_write_pin(&g_pca_led, BUS_SWITCH_PCA_PORT, BUS_SWITCH_PCA_PIN, level);
    if (status != BSP_OK)
    {
        log_e("PCA9555 switch level write failed");
        return status;
    }

    s_bus_physical[BSP_BUS_SPI2_I2S2] = target;
    log_i("Bus switched to %s side", (target == BSP_BUS_OWNER_I2S2) ? "I2S2" : "SPI2");
    return BSP_OK;
}

/**
 * @brief 执行向目标归属方的完整物理与软件切换
 */
static bsp_status_t bus_do_switch(bsp_bus_owner_t target)
{
    bsp_status_t status = BSP_OK;

    if (s_bus_physical[BSP_BUS_SPI2_I2S2] == target)
    {
        return BSP_OK;
    }

    if (target == BSP_BUS_OWNER_I2S2)
    {
        /* SPI2 → I2S2：挂起 IMU 采样 → 反初始化 SPI2 → 切开关 → 初始化 I2S2 */
        (void)bsp_imu_suspend();
        status = port_spi_deinit(PORT_SPI_2);
        if (status != BSP_OK)
        {
            log_e("SPI2 deinit failed when acquiring I2S2");
            return status;
        }
        status = bus_switch_physical(target);
        if (status != BSP_OK)
        {
            return status;
        }
        status = port_i2s_init(PORT_I2S_1);
        if (status != BSP_OK)
        {
            log_e("I2S2 init failed");
        }
    }
    else
    {
        /* I2S2 → SPI2：停止并反初始化 I2S2 → 切开关 → 初始化 SPI2 → 恢复 IMU 采样 */
        status = port_i2s_deinit(PORT_I2S_1);
        if (status != BSP_OK)
        {
            log_e("I2S2 deinit failed when acquiring SPI2");
            return status;
        }
        status = bus_switch_physical(target);
        if (status != BSP_OK)
        {
            return status;
        }
        /* port_spi_init 仅做状态校验，外设寄存器重建须走 CubeMX 生成的 MX 初始化 */
        MX_SPI2_Init();
        status = port_spi_init(PORT_SPI_2);
        if (status != BSP_OK)
        {
            log_e("SPI2 reinit failed");
            return status;
        }
        (void)bsp_imu_resume();
    }
    return status;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t bsp_bus_acquire(bsp_bus_id_t bus, bsp_bus_owner_t owner)
{
    if (bus >= BSP_BUS_MAX || owner == BSP_BUS_OWNER_NONE || owner >= BSP_BUS_OWNER_MAX)
    {
        return BSP_EINVAL;
    }

    uint32_t primask = port_enter_critical();
    if (s_bus_owner[bus] != BSP_BUS_OWNER_NONE && s_bus_owner[bus] != owner)
    {
        port_exit_critical(primask);
        log_w("Bus busy: held by owner %d", (int)s_bus_owner[bus]);
        return BSP_BUSY;
    }
    /* 先占坑再切总线，避免切换耗时窗口内被并发抢占 */
    s_bus_owner[bus] = owner;
    port_exit_critical(primask);


    /* 首次使用前显式驱动开关控制位，消除 PCA9555 复位后的高阻浮空窗口 */
    if (!s_switch_inited)
    {
        bsp_status_t sw_status = bus_switch_physical(s_bus_physical[bus]);
        if (sw_status != BSP_OK)
        {
            uint32_t rollback = port_enter_critical();
            s_bus_owner[bus] = BSP_BUS_OWNER_NONE;
            port_exit_critical(rollback);
            return sw_status;
        }
        s_switch_inited = true;
    }

    bsp_status_t status = bus_do_switch(owner);
    if (status != BSP_OK)
    {
        /* 切换失败回滚占用权，避免死锁 */
        uint32_t rollback = port_enter_critical();
        s_bus_owner[bus] = BSP_BUS_OWNER_NONE;
        port_exit_critical(rollback);
    }
    return status;
}

bsp_status_t bsp_bus_release(bsp_bus_id_t bus, bsp_bus_owner_t owner)
{
    if (bus >= BSP_BUS_MAX || owner >= BSP_BUS_OWNER_MAX)
    {
        return BSP_EINVAL;
    }

    uint32_t primask = port_enter_critical();
    if (s_bus_owner[bus] == owner)
    {
        s_bus_owner[bus] = BSP_BUS_OWNER_NONE;
    }
    port_exit_critical(primask);
    return BSP_OK;
}

bsp_bus_owner_t bsp_bus_current(bsp_bus_id_t bus)
{
    if (bus >= BSP_BUS_MAX)
    {
        return BSP_BUS_OWNER_NONE;
    }
    return s_bus_owner[bus];
}
