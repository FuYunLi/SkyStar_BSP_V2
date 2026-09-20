/**
 * @file port_sdio.c
 * @brief SDIO 接口层实现
 * @note 隔离底层细节，提供在位预检和容量查询支持。
 */

#include "port_sdio.h"
#include "sdio.h"
#include "bsp_driver_sd.h"
#include "fatfs_platform.h"

/* 标记 SD 卡底层初始化状态 */
static volatile bool s_sdio_initialized = false;

/* 最近一次卡识别失败的逻辑错误码（PORT_SDIO_ERR_* 位掩码） */
static uint32_t s_sdio_error = PORT_SDIO_ERR_NONE;

/* HAL SDMMC 错误位 → 接口层逻辑错误位映射表 */
typedef struct
{
    uint32_t hal_err;  /* HAL 侧 SDMMC_ERROR_* 位 */
    uint32_t port_err; /* 对应的 PORT_SDIO_ERR_* 位 */
} sdio_err_map_t;

static const sdio_err_map_t s_err_map[] =
{
    { SDMMC_ERROR_CMD_CRC_FAIL,           PORT_SDIO_ERR_CMD_CRC      },
    { SDMMC_ERROR_DATA_CRC_FAIL,          PORT_SDIO_ERR_DATA_CRC     },
    { SDMMC_ERROR_CMD_RSP_TIMEOUT,        PORT_SDIO_ERR_CMD_TIMEOUT  },
    { SDMMC_ERROR_DATA_TIMEOUT,           PORT_SDIO_ERR_DATA_TIMEOUT },
    { SDMMC_ERROR_TX_UNDERRUN,            PORT_SDIO_ERR_FIFO         },
    { SDMMC_ERROR_RX_OVERRUN,             PORT_SDIO_ERR_FIFO         },
    { SDMMC_ERROR_ILLEGAL_CMD,            PORT_SDIO_ERR_ILLEGAL_CMD  },
    { SDMMC_ERROR_INVALID_VOLTRANGE,      PORT_SDIO_ERR_VOLT_RANGE   },
    { SDMMC_ERROR_REQUEST_NOT_APPLICABLE, PORT_SDIO_ERR_REQ_NOT_APPL },
    { SDMMC_ERROR_DMA,                    PORT_SDIO_ERR_DMA          },
    { SDMMC_ERROR_TIMEOUT,                PORT_SDIO_ERR_TIMEOUT      },
};

/**
 * @brief 将 HAL 的 SDMMC 错误位掩码翻译为接口层逻辑错误码
 * @note  HAL 错误语义只在本函数内出现，不得向上传播
 * @param hal_error hsd.ErrorCode 原始值
 * @return uint32_t PORT_SDIO_ERR_* 位掩码组合
 */
static uint32_t sdio_translate_error(uint32_t hal_error)
{
    uint32_t mapped = PORT_SDIO_ERR_NONE;
    uint32_t rest = hal_error;

    for (uint32_t i = 0; i < ARRAY_SIZE(s_err_map); i++)
    {
        if ((rest & s_err_map[i].hal_err) != 0U)
        {
            mapped |= s_err_map[i].port_err;
            rest &= ~s_err_map[i].hal_err;
        }
    }

    /* 存在未收录的错误位时给兜底标记，禁止静默丢失 */
    if (rest != SDMMC_ERROR_NONE)
    {
        mapped |= PORT_SDIO_ERR_UNKNOWN;
    }

    return mapped;
}

bsp_status_t port_sdio_init(void)
{
    /* 清上一次记录，避开旧错误码误导本轮诊断 */
    s_sdio_error = PORT_SDIO_ERR_NONE;

    if (s_sdio_initialized)
    {
        return BSP_OK;
    }

    /* 重跑 CubeMX 生成的 SDIO 底座初始化，复位 hsd 句柄配置。
     * 实测（M30 对照实验）：缺少此调用时卡识别阶段响应异常，
     * 该行为与 M28 时代验证通过的旧实现一致，勿删。 */
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
        /* 在下一次调用覆盖句柄前抓取并翻译错误现场 */
        s_sdio_error = sdio_translate_error(hsd.ErrorCode);
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

uint32_t port_sdio_get_error(void)
{
    return s_sdio_error;
}

