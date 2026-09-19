/**
 * @file port_i2s.h
 * @brief I2S 物理层抽象接口头文件
 * @note 遵循 SkyStar BSP V2 规范，隔离 HAL 库，提供 I2S 循环 DMA 流式发送接口。
 *       I2S2 与 SPI2 为同一外设实例的两种工作模式，模式切换与总线归属由
 *       Board 层 bsp_bus 仲裁器统一裁决，本层不感知总线冲突。
 */

#ifndef __PORT_I2S_H
#define __PORT_I2S_H

#include "bsp_board.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 逻辑通道定义
 * ================================================================ */

typedef enum
{
    PORT_I2S_1 = 0U,    /* I2S2（SPI2 外设 I2S 模式），连接 ES8388 */
    PORT_I2S_MAX
} port_i2s_id_t;

/**
 * @brief I2S 流事件回调
 * @note  在 DMA 中断上下文中执行，回调内禁止阻塞与耗时操作，仅置标志/投递事件。
 * @param user_ctx 注册时透传的用户指针
 */
typedef void (*port_i2s_cb_t)(void *user_ctx);

/* ================================================================
 * API 声明
 * ================================================================ */

/**
 * @brief 初始化指定 I2S 通道（含 PLLI2S 时钟、GPIO、循环 DMA 与 NVIC）
 * @note  前置条件：该外设实例的 SPI 模式已由 bsp_bus 仲裁器反初始化。
 * @param id I2S 逻辑 ID
 * @return bsp_status_t 执行结果
 */
bsp_status_t port_i2s_init(port_i2s_id_t id);

/**
 * @brief 反初始化指定 I2S 通道并释放 DMA/NVIC 资源
 * @param id I2S 逻辑 ID
 * @return bsp_status_t 执行结果
 */
bsp_status_t port_i2s_deinit(port_i2s_id_t id);

/**
 * @brief 启动循环 DMA 流式发送
 * @note  缓冲区被外设循环读取，上层通过半传输/全传输回调交替补充数据。
 * @param id        I2S 逻辑 ID
 * @param buf       发送缓冲区（16 位样本数组，生命周期须覆盖整个播放过程）
 * @param samples   缓冲区 16 位样本总数（半传输/全传输各对应 samples/2）
 * @param half_cb   半传输完成回调（前半缓冲已用完），可为 NULL
 * @param full_cb   全传输完成回调（后半缓冲已用完），可为 NULL
 * @param user_ctx  透传给回调的用户指针
 * @return bsp_status_t 执行结果
 */
bsp_status_t port_i2s_start_dma(port_i2s_id_t id, const uint16_t *buf,
                                size_t samples,
                                port_i2s_cb_t half_cb, port_i2s_cb_t full_cb,
                                void *user_ctx);

/**
 * @brief 停止 DMA 流式发送
 * @param id I2S 逻辑 ID
 * @return bsp_status_t 执行结果
 */
bsp_status_t port_i2s_stop(port_i2s_id_t id);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_I2S_H */
