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
 * 类型定义
 * ================================================================ */

/**
 * @brief 自定义 SD 卡物理参数结构体（隔离 HAL 库类型）
 */
typedef struct
{
    uint32_t CardType;      /* 卡类型 */
    uint32_t CardVersion;   /* 卡版本 */
    uint32_t BlockSize;     /* 块大小（字节） */
    uint32_t BlockNbr;      /* 总块数 */
} port_sdio_card_info_t;

/* ================================================================
 * 诊断错误码（HAL 的 SDMMC 错误语义已在接口层翻译，上层无需认识 HAL）
 * ================================================================ */

#define PORT_SDIO_ERR_NONE          (0x00000000U)  /* 无错误 */
#define PORT_SDIO_ERR_CMD_CRC       (0x00000001U)  /* 命令响应 CRC 校验失败 */
#define PORT_SDIO_ERR_DATA_CRC      (0x00000002U)  /* 数据块 CRC 校验失败 */
#define PORT_SDIO_ERR_CMD_TIMEOUT   (0x00000004U)  /* 命令应答超时（卡未响应：供电、时钟、总线宽度或开关选通） */
#define PORT_SDIO_ERR_DATA_TIMEOUT  (0x00000008U)  /* 数据应答超时 */
#define PORT_SDIO_ERR_FIFO          (0x00000010U)  /* SDIO FIFO 溢出/下溢 */
#define PORT_SDIO_ERR_ILLEGAL_CMD   (0x00000020U)  /* 当前卡状态下命令非法 */
#define PORT_SDIO_ERR_VOLT_RANGE    (0x00000040U)  /* 电压窗口不被卡支持（ACMD41 认证失败） */
#define PORT_SDIO_ERR_REQ_NOT_APPL  (0x00000080U)  /* 命令请求不适用（卡未就绪或未识别） */
#define PORT_SDIO_ERR_DMA           (0x00000100U)  /* DMA 传输错误 */
#define PORT_SDIO_ERR_TIMEOUT       (0x00000200U)  /* 底层操作超时 */
#define PORT_SDIO_ERR_UNKNOWN       (0x80000000U)  /* 存在未收录的 HAL 错误位，需查接口层映射表 */

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
 * @brief 反初始化 SDIO 接口层
 * @return bsp_status_t
 *         - BSP_OK: 成功
 *         - BSP_ERROR: 底层反初始化失败
 */
bsp_status_t port_sdio_deinit(void);

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
bsp_status_t port_sdio_get_card_info(port_sdio_card_info_t *card_info);

/**
 * @brief 获取最近一次卡识别/初始化的失败原因
 * @note  仅在 port_sdio_init() 返回 BSP_ERROR 后有参考价值；
 *        调用 port_sdio_init() 会清除上一次记录。
 * @return uint32_t PORT_SDIO_ERR_* 位掩码组合，无错误时为 PORT_SDIO_ERR_NONE
 */
uint32_t port_sdio_get_error(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_SDIO_H */
