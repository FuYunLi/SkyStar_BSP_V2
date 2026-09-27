/**
 * @file bsp_ota.c
 * @brief 板级 OTA 更新器服务层实现
 * @note 数据落点：W25Q128 前 4MB OTA 区（头部 0x0 独占 4KB 扇区，
 *       镜像 0x4000 起）。与 LittleFS（4MB 起）物理隔离，零冲突。
 *       CRC 流式累计于 write 阶段，commit 时终算写入头部。
 *       【未验证】OTA 闭环尚未上板实测。
 */

#define LOG_TAG "BSP_OTA"

#include "bsp_ota.h"
#include "bsp_logger.h"
#include "dev_w25q.h"
#include "ota_image.h"
#include <string.h>

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define OTA_CRC32_INIT      (0xFFFFFFFFUL)
#define OTA_CRC32_POLY      (0xEDB88320UL)
#define OTA_CHUNK           (512U)   /* 头部校验用读取块 */

/* ================================================================
 * 私有变量
 * ================================================================ */

static bool     s_session_active;
static uint32_t s_declared_size;
static uint32_t s_written;
static uint32_t s_running_crc;

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 头部重写（头部独占 4KB 扇区：擦后重编程）
 */
static bsp_status_t s_ota_write_header(const ota_header_t *hdr)
{
    bsp_status_t ret = dev_w25q_erase_sector(OTA_HEADER_ADDR);
    if (ret != BSP_OK)
    {
        return ret;
    }

    (void)dev_w25q_sync();

    return dev_w25q_write(OTA_HEADER_ADDR, (const uint8_t *)hdr, sizeof(ota_header_t));
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 开始固件更新会话
 */
bsp_status_t bsp_ota_begin(uint32_t image_size)
{
    if (s_session_active)
    {
        return BSP_BUSY;
    }

    if ((image_size == 0U) || (image_size > OTA_IMAGE_MAX))
    {
        return BSP_EINVAL;
    }

    ota_header_t hdr;
    bsp_status_t ret;

    /* 1. 擦头部扇区 + 镜像数据覆盖扇区（0x4000 起按 4KB 粒度） */
    ret = dev_w25q_erase_sector(OTA_HEADER_ADDR);
    if (ret != BSP_OK)
    {
        return ret;
    }

    uint32_t image_sectors = (image_size + 4095U) / 4096U;
    for (uint32_t i = 0U; i < image_sectors; i++)
    {
        if ((i % 16U) == 0U)
        {
            log_i("erasing %lu/%lu sectors...", (unsigned long)i, (unsigned long)image_sectors);
        }

        ret = dev_w25q_erase_sector(OTA_IMAGE_ADDR + (i * 4096U));
        if (ret != BSP_OK)
        {
            return ret;
        }
    }
    (void)dev_w25q_sync();
    log_i("erase done (%lu sectors).", (unsigned long)image_sectors);

    /* 2. 写 DOWNLOADING 头（此时断电重启 = bootloader 视为无效，安全） */
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = OTA_MAGIC;
    hdr.image_size = image_size;
    hdr.image_crc = 0U;
    hdr.version = OTA_APP_VERSION;
    hdr.state = OTA_STATE_DOWNLOADING;

    ret = s_ota_write_header(&hdr);
    if (ret != BSP_OK)
    {
        return ret;
    }

    s_declared_size = image_size;
    s_written = 0U;
    s_running_crc = OTA_CRC32_INIT;
    s_session_active = true;

    log_i("ota session ready, size=%lu", (unsigned long)image_size);
    return BSP_OK;
}

/**
 * @brief 顺序追加镜像数据
 */
bsp_status_t bsp_ota_write(const uint8_t *chunk, uint32_t len)
{
    if (!s_session_active)
    {
        return BSP_EINVAL;
    }

    if ((chunk == NULL) || (len == 0U))
    {
        return BSP_EINVAL;
    }

    if (s_written + len > s_declared_size)
    {
        return BSP_ERROR; /* 超写：会话数据已不可信 */
    }

    bsp_status_t ret = dev_w25q_write(OTA_IMAGE_ADDR + s_written, chunk, len);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 流式 CRC 累加（与 Bootloader 逐位实现参数一致） */
    for (uint32_t i = 0U; i < len; i++)
    {
        s_running_crc ^= (uint32_t)chunk[i];
        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            s_running_crc = ((s_running_crc & 1UL) != 0UL)
                                ? ((s_running_crc >> 1) ^ OTA_CRC32_POLY)
                                : (s_running_crc >> 1);
        }
    }

    s_written += len;
    return BSP_OK;
}

/**
 * @brief 提交更新
 */
bsp_status_t bsp_ota_commit(void)
{
    if (!s_session_active)
    {
        return BSP_EINVAL;
    }

    if (s_written != s_declared_size)
    {
        log_e("commit rejected: %lu/%lu bytes", (unsigned long)s_written,
              (unsigned long)s_declared_size);
        return BSP_ERROR;
    }

    ota_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = OTA_MAGIC;
    hdr.image_size = s_written;
    hdr.image_crc = s_running_crc ^ 0xFFFFFFFFUL;
    hdr.version = OTA_APP_VERSION;
    hdr.state = OTA_STATE_READY;

    bsp_status_t ret = s_ota_write_header(&hdr);
    if (ret != BSP_OK)
    {
        return ret;
    }

    s_session_active = false;
    log_i("committed: crc=0x%08lX, READY. reboot to apply.",
          (unsigned long)hdr.image_crc);
    return BSP_OK;
}

/**
 * @brief 放弃会话
 */
bsp_status_t bsp_ota_abort(void)
{
    if (!s_session_active)
    {
        return BSP_OK;
    }

    ota_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.state = OTA_STATE_EMPTY;

    s_session_active = false;
    return s_ota_write_header(&hdr);
}
