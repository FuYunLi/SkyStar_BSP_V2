/**
 * @file port_i2s.h
 * @brief I2S 物理层接口头文件
 * @note 封装 STM32F4 I2S 外设底层的配置与 DMA 双缓冲发送逻辑
 */

#ifndef __PORT_I2S_H
#define __PORT_I2S_H

#include "bsp_board.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 音频事件回调类型 */
typedef void (*port_i2s_cb_t)(void);

/**
 * @brief  初始化 I2S 物理外设及其 DMA、引脚和音频时钟
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_i2s_init(void);

/**
 * @brief  释放 I2S 物理外设并关闭 DMA 发送
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_i2s_deinit(void);

/**
 * @brief  注册音频传输半完成与全完成的中断回调函数
 * @param  half_cb 半传输完成回调
 * @param  cplt_cb 传输完成回调
 */
void port_i2s_register_callbacks(port_i2s_cb_t half_cb, port_i2s_cb_t cplt_cb);

/**
 * @brief  开始 I2S DMA 双缓冲推流发送
 * @param  buffer 环形推流缓冲区首地址
 * @param  len 缓冲区半字长度（注意：以 16-bit 长度为单位）
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_i2s_write_dma(uint16_t *buffer, uint16_t len);

/**
 * @brief  停止 I2S DMA 音频传输
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_i2s_stop(void);

/**
 * @brief  动态设置 I2S 采样率
 * @param  sample_rate 采样率频率 (Hz)
 * @retval bsp_status_t 成功返回 BSP_OK
 */
bsp_status_t port_i2s_set_sample_rate(uint32_t sample_rate);

/**
 * @brief  检查 I2S 是否处于忙（传输中）状态
 * @retval bool 忙返回 true
 */
bool port_i2s_is_busy(void);


#ifdef __cplusplus
}
#endif

#endif /* __PORT_I2S_H */
