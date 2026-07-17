/**
 * @file port_sdio.c
 * @brief SDIO 接口层实现
 * @note 隔离底层细节，提供在位预检和容量查询支持
 */

#include "port_sdio.h"
#include "sdio.h"
#include "bsp_driver_sd.h"
#include "fatfs_platform.h"

/* 标记 SD 卡底层初始化状态 */
static volatile bool s_sdio_initialized = false;

bsp_status_t port_sdio_init(void)
{
    if (s_sdio_initialized)
    {
        return BSP_OK;
    }

    /* 校验底层 SDIO 控制句柄是否已初始化 */
    if (hsd.Instance != SDIO)
    {
        return BSP_ERROR;
    }
    
    /* 防爆预检：如果卡未插入，直接返回不强行初始化 */
    if (!port_sdio_is_present())
    {
        s_sdio_initialized = false;
        return BSP_ENODEV;
    }
    
    /* 执行协议层卡识别与初始化 */
    uint8_t res = BSP_SD_Init();
    if (res != MSD_OK)
    {
        s_sdio_initialized = false;
        return BSP_ERROR;
    }
    
    s_sdio_initialized = true;
    return BSP_OK;
}

bsp_status_t port_sdio_deinit(void)
{
    if (!s_sdio_initialized)
    {
        return BSP_OK;
    }

    /* 调用 HAL 库 SD 反初始化底层 */
    if (HAL_SD_DeInit(&hsd) != HAL_OK)
    {
        return BSP_ERROR;
    }

    s_sdio_initialized = false;
    return BSP_OK;
}

bool port_sdio_is_present(void)
{
    /* 调用 BSP 平台的物理引脚 (PD3) 检测函数 */
    bool present = (BSP_PlatformIsDetected() == SD_PRESENT);
    if (!present)
    {
        s_sdio_initialized = false;
    }
    return present;
}

bsp_status_t port_sdio_get_card_info(port_sdio_card_info_t *card_info)
{
    BSP_CHECK_NULL(card_info);
    
    if (!s_sdio_initialized || !port_sdio_is_present())
    {
        return BSP_ENODEV;
    }
    
    HAL_SD_CardInfoTypeDef hal_info;
    BSP_SD_GetCardInfo(&hal_info);

    card_info->CardType = hal_info.CardType;
    card_info->CardVersion = hal_info.CardVersion;
    card_info->BlockSize = hal_info.BlockSize;
    card_info->BlockNbr = hal_info.BlockNbr;

    return BSP_OK;
}

