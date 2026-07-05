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
    /* 调用 CubeMX 自动生成的 SDIO 及相关 DMA 底座初始化 */
    MX_SDIO_SD_Init();

    /* 协议层识别卡前，必须将总线宽度强制重设为 1-bit 模式 */
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
    
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

bool port_sdio_is_present(void)
{
    /* 调用 BSP 平台的物理引脚 (PD3) 检测函数 */
    return (BSP_PlatformIsDetected() == SD_PRESENT);
}

bsp_status_t port_sdio_get_card_info(HAL_SD_CardInfoTypeDef *card_info)
{
    BSP_CHECK_NULL(card_info);
    
    if (!s_sdio_initialized || !port_sdio_is_present())
    {
        return BSP_ENODEV;
    }
    
    BSP_SD_GetCardInfo(card_info);
    return BSP_OK;
}
