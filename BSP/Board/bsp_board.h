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

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 调试开关 ==================== */

#ifndef BSP_DEBUG
#define BSP_DEBUG 1
#endif

/* ==================== 超时常量 ==================== */

#define BSP_WAIT_FOREVER (0xFFFFFFFFU)
#define BSP_NO_WAIT      (0U)

/* ==================== 状态码定义 ==================== */

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

/* ==================== 状态码字符串 ==================== */

const char *bsp_status_str(bsp_status_t status);

/* ==================== 断言宏 ==================== */

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

/* ==================== 参数检查宏 ==================== */

#define BSP_CHECK_NULL(ptr)            BSP_ASSERT((ptr) != NULL)
#define BSP_CHECK_RANGE(val, min, max) BSP_ASSERT((val) >= (min) && (val) <= (max))

/* ==================== 数组与位操作 ==================== */

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define BIT(n)          (1U << (n))

/* ==================== 最值比较 ==================== */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* ==================== 字节操作（协议解析常用） ==================== */

#define HI_BYTE(x)       ((uint8_t)((x) >> 8))
#define LO_BYTE(x)       ((uint8_t)((x) & 0xFF))
#define MAKE_WORD(hi, lo) ((uint16_t)(((hi) << 8) | (lo)))

/* ==================== 容器操作（链表常用） ==================== */

#define CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BOARD_H */
