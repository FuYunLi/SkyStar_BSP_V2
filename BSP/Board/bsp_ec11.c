/**
 * @file bsp_ec11.c
 * @brief EC11 旋转编码器板级支持服务实现源文件
 * @note 封装底板模拟开关选通机制，桥接底层设备驱动，并提供高可靠的获取及重置功能。
 */

#define LOG_TAG "EC11_SRV"

#include "bsp_ec11.h"
#include "dev_ec11.h"
#include "dev_pca9555.h"
#include "bsp_logger.h"

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* 编码器模拟开关引脚配置：Port 0 Pin 5 (对应 SW7 拨码开关第 6 位) */
#define EC11_SWITCH_PORT  (0U)
#define EC11_SWITCH_PIN   (5U)

/* ================================================================
 * 外部实例声明
 * ================================================================ */

/* 全局唯一 PCA9555 芯片硬件实例 (定义在 bsp_led.c 中) */
extern dev_pca9555_t g_pca_led;

/* ================================================================
 * 私有静态变量
 * ================================================================ */

static bool s_is_init = false;          /* 板级服务初始化状态 */

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 EC11 编码器板级服务，配置模拟开关物理通道并启动驱动
 */
bsp_status_t bsp_ec11_init(void)
{
    /* 1. 通过 PCA9555 选通底板编码器模拟通道：配置 Port 0 Pin 5 为输出并写入低电平 (RESET) */
    bsp_status_t status = dev_pca9555_set_pin_dir(&g_pca_led, EC11_SWITCH_PORT, EC11_SWITCH_PIN, 0);
    if (status != BSP_OK)
    {
        log_e("Failed to config PCA9555 switch pin for EC11! ret = %d", status);
        return status;
    }

    status = dev_pca9555_write_pin(&g_pca_led, EC11_SWITCH_PORT, EC11_SWITCH_PIN, DEV_PCA9555_RESET);
    if (status != BSP_OK)
    {
        log_e("Failed to write PCA9555 switch pin low for EC11! ret = %d", status);
        return status;
    }
    
    log_i("EC11 physical channel selected via PCA9555 successfully.");

    /* 2. 调用底层设备驱动层初始化引脚及状态机 */
    status = dev_ec11_init();
    if (status != BSP_OK)
    {
        log_e("Failed to init physical EC11 hardware! ret = %d", status);
        return status;
    }

    s_is_init = true;
    log_i("EC11 board support service initialized successfully.");
    
    return BSP_OK;
}

/**
 * @brief 获取编码器的最新旋转参数
 */
bsp_status_t bsp_ec11_get_info(bsp_ec11_info_t *info)
{
    if (info == NULL)
    {
        return BSP_EINVAL;
    }

    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    dev_ec11_info_t dev_info = {0};
    bsp_status_t status = dev_ec11_get_info(&dev_info);
    if (status != BSP_OK)
    {
        return status;
    }

    info->count = dev_info.count;
    info->dir = dev_info.dir;

    return BSP_OK;
}

/**
 * @brief 重置板级编码器服务及底层的计数值与方向参数
 */
bsp_status_t bsp_ec11_reset_count(void)
{
    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    return dev_ec11_reset_count();
}
