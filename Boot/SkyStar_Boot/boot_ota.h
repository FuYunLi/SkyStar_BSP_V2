/**
 * @file boot_ota.h
 * @brief Bootloader OTA 流水线头文件（校验/擦写/拷贝/跳转）
 */

#ifndef __BOOT_OTA_H
#define __BOOT_OTA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 软件计算 CRC-32（ISO-HDLC 参数，与 PC 端 crcmap 工具一致）
 */
uint32_t boot_crc32(const uint8_t *data, uint32_t len);

/**
 * @brief OTA 主流程：读头部 → 校验 → 擦片上 → 拷贝 → CRC 复验 → 跳转
 * @note 无论镜像是否有效，函数最终都会尝试跳转 App（v1 无回滚，
 *       无有效镜像时按"现状启动"处理并打印原因）
 */
void boot_ota_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_OTA_H */
