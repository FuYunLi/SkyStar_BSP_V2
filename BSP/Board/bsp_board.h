/**
 * @file bsp_board.h
 * @brief 板级支持包核心定义
 * @note 包含状态码、断言宏、常用宏等基础定义
 */

#ifndef __BSP_BOARD_H
#define __BSP_BOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 调试开关
 * ================================================================ */

#ifndef BSP_DEBUG
#define BSP_DEBUG 1
#endif

/* ================================================================
 * 超时常量
 * ================================================================ */

#define BSP_WAIT_FOREVER (0xFFFFFFFFU)
#define BSP_NO_WAIT      (0U)

/* ================================================================
 * 状态码定义
 * ================================================================ */

typedef enum
{
    BSP_OK       =  0,
    BSP_ERROR    = -1,
    BSP_EINVAL   = -2,
    BSP_ENOMEM   = -3,
    BSP_ETIMEOUT = -4,
    BSP_BUSY     = -5,
    BSP_EIO      = -6,
    BSP_ENODEV   = -7,
    BSP_STATUS_MAX
} bsp_status_t;

/* ================================================================
 * HAL 状态转换
 * ================================================================ */

static inline bsp_status_t hal_to_bsp_status(HAL_StatusTypeDef hal_status)
{
    switch (hal_status)
    {
        case HAL_OK:      return BSP_OK;
        case HAL_ERROR:   return BSP_ERROR;
        case HAL_BUSY:    return BSP_BUSY;
        case HAL_TIMEOUT: return BSP_ETIMEOUT;
        default:          return BSP_ERROR;
    }
}

/* ================================================================
 * 状态码字符串
 * ================================================================ */

const char *bsp_status_str(bsp_status_t status);

/* ================================================================
 * 统一异步完成回调类型
 * ================================================================ */

/**
 * @brief 统一异步完成回调
 *
 * @param bus_id   发起操作的总线逻辑 ID（枚举强转为 uint8_t）
 * @param result   操作结果，BSP_OK 表示成功
 * @param user_ctx 调用方在注册时传入的透明指针，驱动原样回传
 *
 * 使用示例：
 *   void my_cb(uint8_t id, bsp_status_t r, void *ctx) {
 *       if (r == BSP_OK) { ... }
 *   }
 */
typedef void (*port_async_cb_t)(uint8_t bus_id, bsp_status_t result, void *user_ctx);

/* ================================================================
 * 断言宏
 * ================================================================ */

#if BSP_DEBUG
#define BSP_ASSERT(expr)               \
    do {                               \
        if (!(expr)) {                 \
            return BSP_EINVAL;         \
        }                              \
    } while(0)
#else
#define BSP_ASSERT(expr) ((void)0)
#endif

/* ================================================================
 * 参数检查宏
 * ================================================================ */

#define BSP_CHECK_NULL(ptr)            BSP_ASSERT((ptr) != NULL)
#define BSP_CHECK_RANGE(val, min, max) BSP_ASSERT((val) >= (min) && (val) <= (max))

/* ================================================================
 * 数组与位操作
 * ================================================================ */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BIT(n)          (1U << (n))

/* ================================================================
 * 最值比较
 * ================================================================ */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* ================================================================
 * 字节操作
 * ================================================================ */

#define HI_BYTE(x)       ((uint8_t)((x) >> 8))
#define LO_BYTE(x)       ((uint8_t)((x) & 0xFF))
#define MAKE_WORD(hi, lo) ((uint16_t)(((hi) << 8) | (lo)))

/* ================================================================
 * 容器操作
 * ================================================================ */

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BOARD_H */
