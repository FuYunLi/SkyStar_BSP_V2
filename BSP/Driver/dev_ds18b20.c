/**
 * @file dev_ds18b20.c
 * @brief DS18B20 数字温度传感器驱动实现
 * @note 命令序列：复位应答 → Skip ROM(0xCC) → ConvertT(0x44) →
 *       等待转换 → 复位应答 → Skip ROM(0xCC) → Read Scratchpad(0xBE)
 *       → 读 9 字节暂存器（前 2 字节为温度补码）→ CRC8 校验。
 *       【未验证】外部传感器尚未接线实测。
 */

#include "dev_ds18b20.h"
#include "soft_onewire.h"
#include "port_dwt.h"

/* ================================================================
 * 私有宏定义与常量
 * ================================================================ */

#define DS18B20_CMD_SKIP_ROM      (0xCCU)
#define DS18B20_CMD_CONVERT_T     (0x44U)
#define DS18B20_CMD_READ_SP       (0xBEU)
#define DS18B20_SP_SIZE           (9U)     /* 暂存器 9 字节（8 数据 + CRC） */
#define DS18B20_CONVERT_12BIT_MS  (750U)   /* 12 位分辨率转换时间 */

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief Dallas CRC8 校验（多项式 x^8+x^5+x^4+1，反射 0x8C）
 */
static uint8_t s_ds18b20_crc8(const uint8_t *data, uint32_t len)
{
    uint8_t crc = 0U;

    for (uint32_t i = 0U; i < len; i++)
    {
        uint8_t cur = data[i];

        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            uint8_t mix = (uint8_t)((crc ^ cur) & 0x01U);
            crc >>= 1;

            if (mix != 0U)
            {
                crc ^= 0x8CU;
            }

            cur >>= 1;
        }
    }

    return crc;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化单总线引脚
 */
bsp_status_t dev_ds18b20_init(void)
{
    return soft_onewire_init(PORT_OW_1);
}

/**
 * @brief 启动一次温度转换并读取结果
 */
bsp_status_t dev_ds18b20_read_temp(int16_t *temp_deci_c)
{
    if (temp_deci_c == NULL)
    {
        return BSP_EINVAL;
    }

    bool presence = false;

    /* 1. 复位 + Skip ROM + 启动转换 */
    bsp_status_t ret = soft_onewire_reset(PORT_OW_1, &presence);
    if ((ret != BSP_OK) || (!presence))
    {
        return BSP_ENODEV;
    }

    (void)soft_onewire_write_byte(PORT_OW_1, DS18B20_CMD_SKIP_ROM);
    (void)soft_onewire_write_byte(PORT_OW_1, DS18B20_CMD_CONVERT_T);

    /* 转换期间可读 0，完成恢复 1——简化为固定等待 12 位最长转换时间 */
    port_dwt_delay_us(DS18B20_CONVERT_12BIT_MS * 1000U);

    /* 2. 复位 + Skip ROM + 读暂存器 */
    ret = soft_onewire_reset(PORT_OW_1, &presence);
    if ((ret != BSP_OK) || (!presence))
    {
        return BSP_ENODEV;
    }

    (void)soft_onewire_write_byte(PORT_OW_1, DS18B20_CMD_SKIP_ROM);
    (void)soft_onewire_write_byte(PORT_OW_1, DS18B20_CMD_READ_SP);

    uint8_t scratchpad[DS18B20_SP_SIZE] = {0};
    for (uint32_t i = 0U; i < DS18B20_SP_SIZE; i++)
    {
        ret = soft_onewire_read_byte(PORT_OW_1, &scratchpad[i]);
        if (ret != BSP_OK)
        {
            return ret;
        }
    }

    /* 3. CRC 校验：前 8 字节计算结果须与第 9 字节一致 */
    if (s_ds18b20_crc8(scratchpad, DS18B20_SP_SIZE - 1U) != scratchpad[DS18B20_SP_SIZE - 1U])
    {
        return BSP_ERROR;
    }

    /* 4. 温度换算：16 位补码，1 LSB = 1/16 °C */
    int16_t raw = (int16_t)((uint16_t)scratchpad[0] | ((uint16_t)scratchpad[1] << 8));

    *temp_deci_c = (int16_t)(((int32_t)raw * 10) / 16);

    return BSP_OK;
}
