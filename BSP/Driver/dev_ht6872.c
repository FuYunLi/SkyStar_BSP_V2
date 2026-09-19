/**
 * @file dev_ht6872.c
 * @brief HT6872 D类音频功放使能控制驱动实现源文件
 * @note  使能脚经 PCA9555 输出。引脚号推算依据：bsp_imu 原实现确认
 *        "Port0 Pin2 ↔ 拨码 BIT3"，按位序对齐推算 BIT1 ↔ Port0 Pin0，
 *        上板以 audio_pa 命令实测核实后如有出入仅需调整下方宏。
 */

#define LOG_TAG "DEV_HT6872"

#include "dev_ht6872.h"
#include "dev_pca9555.h"
#include "bsp_logger.h"

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* 功放使能位：拨码 BIT1 ↔ PCA9555 Port0 Pin0（推算，上板核实） */
#define HT6872_PCA_PORT    (0U)
#define HT6872_PCA_PIN     (0U)

/* 全局唯一 PCA9555 物理芯片实例（定义于 bsp_led.c） */
extern dev_pca9555_t g_pca_led;

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t dev_ht6872_init(void)
{
    bsp_status_t status = dev_pca9555_set_pin_dir(&g_pca_led, HT6872_PCA_PORT,
                                                  HT6872_PCA_PIN, 0);
    if (status != BSP_OK)
    {
        log_e("PCA9555 amp pin dir config failed");
        return status;
    }

    /* 默认静音：与板上 1K 下拉的硬件保底状态一致 */
    status = dev_pca9555_write_pin(&g_pca_led, HT6872_PCA_PORT, HT6872_PCA_PIN, DEV_PCA9555_RESET);
    if (status != BSP_OK)
    {
        log_e("PCA9555 amp pin write failed");
        return status;
    }
    log_i("HT6872 amp channel registered (default muted)");
    return BSP_OK;
}

bsp_status_t dev_ht6872_enable(bool en)
{
    return dev_pca9555_write_pin(&g_pca_led, HT6872_PCA_PORT, HT6872_PCA_PIN, en ? DEV_PCA9555_SET : DEV_PCA9555_RESET);
}
