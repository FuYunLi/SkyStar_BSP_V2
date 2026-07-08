/**
 * @file dev_es8388.h
 * @brief ES8388 音频编解码器驱动头文件
 * @note 面向对象设计，支持 I2C 控制接口配置，支持音量和通道管理
 */

#ifndef __DEV_ES8388_H
#define __DEV_ES8388_H

#include "bsp_board.h"
#include "port_i2c.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * ES8388 寄存器宏定义
 * ================================================================ */
#define ES8388_CONTROL1          0x00U  /* 系统控制 1 */
#define ES8388_CONTROL2          0x01U  /* 系统控制 2 */
#define ES8388_CHIPPOWER         0x02U  /* 芯片电源管理 */
#define ES8388_ADCPOWER          0x03U  /* ADC 电源管理 */
#define ES8388_DACPOWER          0x04U  /* DAC 电源管理 */
#define ES8388_CHIPLOPOW1        0x05U  /* 芯片低功耗 1 */
#define ES8388_CHIPLOPOW2        0x06U  /* 芯片低功耗 2 */
#define ES8388_ANAVOLMANAG       0x07U  /* 模拟音量管理（LOUT1/ROUT1/LOUT2/ROUT2 模拟增益基准）*/
#define ES8388_MASTERMODE        0x08U  /* 主从模式与时钟设置 */

/* ADC 寄存器 */
#define ES8388_ADCCONTROL1       0x09U
#define ES8388_ADCCONTROL2       0x0AU
#define ES8388_ADCCONTROL3       0x0BU
#define ES8388_ADCCONTROL4       0x0CU
#define ES8388_ADCCONTROL5       0x0DU
#define ES8388_ADCCONTROL6       0x0EU
#define ES8388_ADCCONTROL7       0x0FU
#define ES8388_ADCCONTROL8       0x10U
#define ES8388_ADCCONTROL9       0x11U
#define ES8388_ADCCONTROL10      0x12U
#define ES8388_ADCCONTROL11      0x13U
#define ES8388_ADCCONTROL12      0x14U
#define ES8388_ADCCONTROL13      0x15U
#define ES8388_ADCCONTROL14      0x16U

/* DAC 寄存器 */
#define ES8388_DACCONTROL1       0x17U
#define ES8388_DACCONTROL2       0x18U
#define ES8388_DACCONTROL3       0x19U
#define ES8388_DACCONTROL4       0x1AU
#define ES8388_DACCONTROL5       0x1BU
#define ES8388_DACCONTROL6       0x1CU
#define ES8388_DACCONTROL7       0x1DU
#define ES8388_DACCONTROL8       0x1EU
#define ES8388_DACCONTROL9       0x1FU
#define ES8388_DACCONTROL10      0x20U
#define ES8388_DACCONTROL11      0x21U
#define ES8388_DACCONTROL12      0x22U
#define ES8388_DACCONTROL13      0x23U
#define ES8388_DACCONTROL14      0x24U
#define ES8388_DACCONTROL15      0x25U
#define ES8388_DACCONTROL16      0x26U  /* 左/右声道混音器源选择 */
#define ES8388_DACCONTROL17      0x27U  /* 左声道混音器增益与控制 */
#define ES8388_DACCONTROL18      0x28U
#define ES8388_DACCONTROL19      0x29U
#define ES8388_DACCONTROL20      0x2AU  /* LOUT1 混音器路由选择（Bit7: LD2LO DAC路由, Bit3: LI2LO 输入路由）*/
#define ES8388_DACCONTROL21      0x2BU  /* 时钟/LRCK 路由配置（SLRCK/LRCK_SEL 等，注意非音量寄存器）*/
#define ES8388_DACCONTROL22      0x2CU  /* ROUT1 混音器路由选择 */
#define ES8388_DACCONTROL23      0x2DU  /* ROUT2 混音器路由选择 */
#define ES8388_DACCONTROL24      0x2EU  /* LOUT1VOL: LOUT1 模拟输出驱动器音量，0x00=-45dB, 0x1E=0dB, 0x24=+6dB */
#define ES8388_DACCONTROL25      0x2FU  /* ROUT1VOL: ROUT1 模拟输出驱动器音量，0x00=-45dB, 0x1E=0dB, 0x24=+6dB */
#define ES8388_DACCONTROL26      0x30U  /* LOUT2VOL: LOUT2 模拟输出驱动器音量（喇叭通道）*/
#define ES8388_DACCONTROL27      0x31U  /* ROUT2VOL: ROUT2 模拟输出驱动器音量（喇叭通道）*/
#define ES8388_DACCONTROL28      0x32U
#define ES8388_DACCONTROL29      0x33U
#define ES8388_DACCONTROL30      0x34U

/* ES8388 驱动结构体 */
typedef struct
{
    port_i2c_id_t i2c_id;    /* 使用的 I2C 逻辑端口 ID */
    uint8_t       dev_addr;  /* 8位 I2C 写地址 */
} dev_es8388_t;

/* ================================================================
 * API 接口声明
 * ================================================================ */

/**
 * @brief 初始化 ES8388 音频编解码器
 * @param dev 驱动结构体指针
 * @param i2c_id I2C 端口 ID
 * @param dev_addr I2C 物理写地址 (一般为 0x20)
 * @retval BSP_OK 初始化成功
 * @retval 其他 初始化失败
 */
bsp_status_t dev_es8388_init(dev_es8388_t *dev, port_i2c_id_t i2c_id, uint8_t dev_addr);

/**
 * @brief 设置播放音量
 * @param dev 驱动结构体指针
 * @param volume 音量百分比 (0 - 100)
 * @retval BSP_OK 设置成功
 * @retval 其他 设置失败
 */
bsp_status_t dev_es8388_set_volume(dev_es8388_t *dev, uint8_t volume);

/**
 * @brief 开启静音
 * @param dev 驱动结构体指针
 * @retval BSP_OK 设置成功
 * @retval 其他 设置失败
 */
bsp_status_t dev_es8388_mute(dev_es8388_t *dev);

/**
 * @brief 关闭静音
 * @param dev 驱动结构体指针
 * @retval BSP_OK 设置成功
 * @retval 其他 设置失败
 */
bsp_status_t dev_es8388_unmute(dev_es8388_t *dev);

/**
 * @brief 写入 ES8388 寄存器
 * @param dev 驱动结构体指针
 * @param reg 目标寄存器地址
 * @param val 要写入的数值
 * @retval BSP_OK 写入成功
 * @retval 其他 写入失败
 */
bsp_status_t dev_es8388_write_reg(dev_es8388_t *dev, uint8_t reg, uint8_t val);

/**
 * @brief 读取 ES8388 寄存器
 * @param dev 驱动结构体指针
 * @param reg 目标寄存器地址
 * @param val 用于存储读取数值的指针
 * @retval BSP_OK 读取成功
 * @retval 其他 读取失败
 */
bsp_status_t dev_es8388_read_reg(dev_es8388_t *dev, uint8_t reg, uint8_t *val);

#ifdef __cplusplus
}
#endif

#endif /* __DEV_ES8388_H */
