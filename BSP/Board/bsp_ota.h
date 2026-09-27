/**
 * @file bsp_ota.h
 * @brief 板级 OTA 更新器服务层头文件（传输无关）
 * @note 三段式契约：begin（擦区+置 DOWNLOADING）→ write（顺序追加）
 *       → commit（CRC 终算 + 置 READY）。传输层（Ymodem/将来 lwIP）
 *       只负责把字节流喂进来；重启后 Bootloader 依据头部状态接管。
 *       镜像头部契约见 Boot/SkyStar_Boot/ota_image.h（双侧共享）。
 *       【未验证】OTA 闭环尚未上板实测。
 */

#ifndef __BSP_OTA_H
#define __BSP_OTA_H

#include "bsp_board.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* App 自报版本号（`ver` 指令显示；升级闭环以此判断新旧） */
#define OTA_APP_VERSION (0x00010001UL) /* v1.0.1 */

/**
 * @brief 开始一次固件更新会话
 * @param image_size 镜像总字节数（来自 Ymodem 文件头）
 * @retval BSP_OK 就绪，可开始 write
 * @retval BSP_EINVAL 尺寸非法（0 或超出 App 区容量）
 * @retval BSP_BUSY 上一会话未收尾
 */
bsp_status_t bsp_ota_begin(uint32_t image_size);

/**
 * @brief 顺序追加写入镜像数据（内部累计 CRC）
 * @note len 必须与调用顺序连续，由传输层保证
 */
bsp_status_t bsp_ota_write(const uint8_t *chunk, uint32_t len);

/**
 * @brief 提交更新：终算 CRC、头部置 READY，重启后 Bootloader 接管
 * @retval BSP_OK 已提交
 * @retval BSP_ERROR 字节数与声明不符或写头部失败
 */
bsp_status_t bsp_ota_commit(void);

/**
 * @brief 放弃当前会话（头部置回 EMPTY，镜像区留待下次 begin 重擦）
 */
bsp_status_t bsp_ota_abort(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_OTA_H */
