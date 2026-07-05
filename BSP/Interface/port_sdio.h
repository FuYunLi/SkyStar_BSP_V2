/**
 * @file port_sdio.h
 * @brief SDIO 接口层头文件
 * @note 封装 STM32 HAL SDIO 与板载 TF_DET 引脚的操作，隔离底层细节
 */

#ifndef PORT_SDIO_H
#define PORT_SDIO_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 初始化与检测 API
 * ================================================================ */

/**
 * @brief 初始化 SDIO 外设及 DMA，如果检测到卡在位则进一步执行物理卡初始化
 * @return bsp_status_t 
 *         - BSP_OK: 初始化成功且卡已准备就绪
 *         - BSP_ENODEV: TF卡未插入，已安全跳过卡协议层初始化
 *         - BSP_ERROR: 底层驱动或硬件通信错误
 */
bsp_status_t port_sdio_init(void);

/**
 * @brief 检测 TF 卡是否物理在位
 * @return true = TF 卡已插入，false = 未检测到 TF 卡
 */
bool port_sdio_is_present(void);

/**
 * @brief 获取当前 SD 卡的底层物理参数信息
 * @param card_info 接收参数的结构体指针
 * @return bsp_status_t 
 *         - BSP_OK: 成功获取
 *         - BSP_EINVAL: 传入指针为空
 *         - BSP_ENODEV: 卡未在位或未初始化
 */
bsp_status_t port_sdio_get_card_info(HAL_SD_CardInfoTypeDef *card_info);

#ifdef __cplusplus
}
#endif

#endif /* PORT_SDIO_H */
