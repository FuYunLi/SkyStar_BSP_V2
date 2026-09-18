/**
 * @file port_i2s.h
 * @brief I2S 音频接口层头文件
 * @note 提供统一的 I2S 主发送（DMA）接口，隔离 HAL 库与引脚细节。
 *       最小版本约定：进入音频模式前，上层保证 SPI2 空闲（I2S2 与
 *       SPI2 为同一外设的两种模式，SW7 BIT3 拨码路由至 ES8388）；
 *       port_i2s_deinit 会将外设恢复为 SPI 模式。
 *       【未验证】音频链路尚未上板实测。
 */

#ifndef __PORT_I2S_H
#define __PORT_I2S_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 类型定义
 * ================================================================ */

typedef enum
{
    /* I2S2 通道（SPI2 复用，ES8388 数据接口） */
    PORT_I2S_2 = 0,
    PORT_I2S_MAX
} port_i2s_id_t;

/* ================================================================
 * 公开接口声明
 * ================================================================ */

/**
 * @brief 初始化 I2S 接口（主发送模式，飞利浦标准，16 位，MCLK 输出开）
 * @note 幂等设计：重复调用会以新采样率重新初始化。
 *       时钟源走 PLLI2S（N=216/R=5 → 86.4MHz），48kHz 误差约 +0.45%、
 *       44.1kHz 约 +2.0%，其余常用采样率 ≤2%。
 * @param id I2S 逻辑 ID
 * @param sample_rate_hz 采样率（8000-48000）
 * @retval BSP_OK 初始化成功
 * @retval BSP_EINVAL 参数无效
 */
bsp_status_t port_i2s_init(port_i2s_id_t id, uint32_t sample_rate_hz);

/**
 * @brief 初始化 I2S 接口（主接收模式，录音方向）
 * @note 参数语义与 port_i2s_init 一致；录音来自 ES8388 ADC。
 */
bsp_status_t port_i2s_init_rx(port_i2s_id_t id, uint32_t sample_rate_hz);

/**
 * @brief 反初始化 I2S 并将外设恢复为 SPI2 模式（W25Q/IMU 可用）
 */
bsp_status_t port_i2s_deinit(port_i2s_id_t id);

/**
 * @brief 以 DMA 方式发送一组 16 位交错采样（异步，完成时回调）
 * @note 回调在 DMA 中断上下文执行，仅允许置标志等轻量操作。
 *       count 为 uint16_t 采样元素个数（立体声交错计 L/R 各一个）。
 * @param id I2S 逻辑 ID
 * @param samples 采样缓冲区指针，须在回调触发前保持有效
 * @param count 采样元素个数
 * @param cb 发送完成回调，NULL 不通知
 * @param user_ctx 透明指针，原样回传
 * @retval BSP_OK 提交成功
 * @retval BSP_EINVAL 参数无效
 * @retval BSP_BUSY 上一笔传输尚未完成
 */
bsp_status_t port_i2s_write_dma(port_i2s_id_t id, const uint16_t *samples, uint16_t count,
                                port_async_cb_t cb, void *user_ctx);

/**
 * @brief 以 DMA 方式接收一组 16 位交错采样（异步，完成时回调）
 * @note 录音方向专用；init_rx 之后可用。
 */
bsp_status_t port_i2s_read_dma(port_i2s_id_t id, uint16_t *samples, uint16_t count,
                               port_async_cb_t cb, void *user_ctx);

/**
 * @brief 查询发送通道是否忙碌
 */
bool port_i2s_is_busy(port_i2s_id_t id);

/**
 * @brief 停止 DMA 发送并关闭 I2S 外设输出
 */
bsp_status_t port_i2s_stop(port_i2s_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_I2S_H */
