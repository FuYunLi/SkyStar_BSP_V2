/**
 * @file bsp_board.c
 * @brief 板级支持包核心实现
 */

#include "bsp_board.h"

const char *bsp_status_str(bsp_status_t status)
{
    switch (status)
    {
        case BSP_OK:       return "OK";
        case BSP_ERROR:    return "ERROR";
        case BSP_EINVAL:   return "EINVAL";
        case BSP_ENOMEM:   return "ENOMEM";
        case BSP_ETIMEOUT: return "ETIMEOUT";
        case BSP_BUSY:     return "BUSY";
        case BSP_EIO:      return "EIO";
        case BSP_ENODEV:   return "ENODEV";
        default:           return "UNKNOWN";
    }
}
