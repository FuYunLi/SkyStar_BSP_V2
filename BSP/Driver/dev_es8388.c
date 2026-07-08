/**
 * @file dev_es8388.c
 * @brief ES8388 音频编解码器驱动实现
 * @note 采用 Allman 代码风格，通过 I2C 接口配置 ES8388 编解码器以支持 DAC 播放
 */

#include "dev_es8388.h"

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

/**
 * @brief 写入 ES8388 寄存器
 */
bsp_status_t dev_es8388_write_reg(dev_es8388_t *dev, uint8_t reg, uint8_t val)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    return port_i2c_mem_write(dev->i2c_id, dev->dev_addr, reg, 1, &val, 1, 100);
}

/**
 * @brief 读取 ES8388 寄存器
 */
bsp_status_t dev_es8388_read_reg(dev_es8388_t *dev, uint8_t reg, uint8_t *val)
{
    if (dev == NULL || val == NULL)
    {
        return BSP_EINVAL;
    }

    return port_i2c_mem_read(dev->i2c_id, dev->dev_addr, reg, 1, val, 1, 100);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 ES8388 音频编解码器
 */
bsp_status_t dev_es8388_init(dev_es8388_t *dev, port_i2c_id_t i2c_id, uint8_t dev_addr)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    dev->i2c_id = i2c_id;
    dev->dev_addr = dev_addr;

    bsp_status_t ret;

    /* ---------------------------------------------------------------
     * 1. 全局复位与电源管理
     * --------------------------------------------------------------- */
    /* CONTROL1: 开启片内偏置电压和参考电压，VMIDSEL=10 (5kΩ 充电路径) */
    ret = dev_es8388_write_reg(dev, ES8388_CONTROL1, 0x05U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* CONTROL2: 先置复位，再释放，触发内部状态机重置 */
    ret = dev_es8388_write_reg(dev, ES8388_CONTROL2, 0x06U);
    if (ret != BSP_OK)
    {
        return ret;
    }
    HAL_Delay(10);

    /* CONTROL2: 释放复位，使能 DLL 与内部基准 */
    ret = dev_es8388_write_reg(dev, ES8388_CONTROL2, 0x1CU);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* CHIPPOWER: 全部电源域上电 (0x00 = 全开启) */
    ret = dev_es8388_write_reg(dev, ES8388_CHIPPOWER, 0x00U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ANAVOLMANAG: 模拟音量管理寄存器，配置基准电平
     * 0x7B = OUT1VOL/OUT2VOL 控制使能，LDAC/RDAC 取消 0dB 衰减 */
    ret = dev_es8388_write_reg(dev, ES8388_ANAVOLMANAG, 0x7BU);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 2. 关闭 ADC（仅启用 DAC 播放，抑制 ADC 本底噪声）
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_ADCPOWER, 0xFFU);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 3. 开启 DAC 与模拟输出通道
     * DACPOWER: Bit5=LOUT1, Bit4=ROUT1, Bit3=LOUT2, Bit2=ROUT2
     * 0x3C = 0011_1100 = LOUT1+ROUT1+LOUT2+ROUT2 全部使能
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_DACPOWER, 0x3CU);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 4. 主从模式：配置为从机，由 STM32 I2S2 提供 LRCK/BCLK
     * MASTERMODE: 0x00 = 从机模式
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_MASTERMODE, 0x00U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 5. DAC 音频接口格式
     * DACCONTROL1: Bit3=1 I2S格式, Bit2:1=00 16-bit = 0x18
     * DACCONTROL2: Bit2:1=01 MCLK=256Fs = 0x02
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL1, 0x18U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL2, 0x02U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 6. DAC 时钟/LRCK 路由配置 (DACCONTROL21 = REG 0x2B)
     * 注意：此寄存器是时钟路由寄存器，不是混音器音量寄存器！
     * 0x80 = Bit7(SLRCK)=1: 选择 LRCK 作为串行时钟参考
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL21, 0x80U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 7. 模拟输出混音器路由配置
     * DACCONTROL20 (0x2A) = LOUT1 mixer: Bit7(LD2LO)=1 DAC-L->LOUT1
     * DACCONTROL22 (0x2C) = ROUT1 mixer: Bit7(RD2RO)=1 DAC-R->ROUT1
     * DACCONTROL23 (0x2D) = ROUT2 mixer: Bit7(RD2RO)=1 DAC-R->ROUT2
     * 0x90 = 1001_0000 = DAC通道路由使能，输入直通增益=0dB
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL20, 0x90U); /* LOUT1 DAC路由 */
    if (ret != BSP_OK)
    {
        return ret;
    }
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL22, 0x90U); /* ROUT1 DAC路由 */
    if (ret != BSP_OK)
    {
        return ret;
    }
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL23, 0x90U); /* ROUT2 DAC路由 */
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 8. ★ 关键修复：设置模拟输出驱动器音量（上电复位值=0x00=-45dB，静音！）
     * DACCONTROL24 (0x2E) = LOUT1VOL: 0x1E = 0dB 耳机左声道
     * DACCONTROL25 (0x2F) = ROUT1VOL: 0x1E = 0dB 耳机右声道
     * DACCONTROL26 (0x30) = LOUT2VOL: 0x1E = 0dB 喇叭左声道
     * DACCONTROL27 (0x31) = ROUT2VOL: 0x1E = 0dB 喇叭右声道
     * 音量映射: 0x00=-45dB, 0x01=-43.5dB, ..., 0x1E=0dB, 0x24=+6dB (最大)
     * --------------------------------------------------------------- */
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL24, 0x1EU); /* LOUT1VOL = 0dB */
    if (ret != BSP_OK)
    {
        return ret;
    }
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL25, 0x1EU); /* ROUT1VOL = 0dB */
    if (ret != BSP_OK)
    {
        return ret;
    }
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL26, 0x1EU); /* LOUT2VOL = 0dB */
    if (ret != BSP_OK)
    {
        return ret;
    }
    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL27, 0x1EU); /* ROUT2VOL = 0dB */
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 9. 数字音量控制（90% → -9.6dB 轻度数字衰减，保留充足动态范围）
     * --------------------------------------------------------------- */
    ret = dev_es8388_set_volume(dev, 90);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* ---------------------------------------------------------------
     * 10. 解除 DAC 数字静音（DACCONTROL3.DACMute = 0）
     * --------------------------------------------------------------- */
    ret = dev_es8388_unmute(dev);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return BSP_OK;
}


/**
 * @brief 设置播放音量
 */
bsp_status_t dev_es8388_set_volume(dev_es8388_t *dev, uint8_t volume)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    if (volume > 100)
    {
        volume = 100;
    }

    /* 寄存器音量映射：
     * 0x00 = 0dB (最大), 每步 0.375dB 衰减
     * 实用范围: 0x00(0dB) ~ 0xC0(-72dB)
     * 映射公式: volume 100% -> 0x00, volume 0% -> 0xC0 (192)
     * 即: dac_val = (uint8_t)((100U - volume) * 1.92f) */
    uint8_t dac_val = (uint8_t)((100U - volume) * 1.92f);

    /* 写入 L/R 真实 DAC 数字音量控制寄存器 (DACCONTROL4/DACCONTROL5) */
    bsp_status_t ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL4, dac_val);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = dev_es8388_write_reg(dev, ES8388_DACCONTROL5, dac_val);
    return ret;
}

/**
 * @brief 开启静音
 */
bsp_status_t dev_es8388_mute(dev_es8388_t *dev)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    uint8_t reg_val = 0;
    bsp_status_t ret = dev_es8388_read_reg(dev, ES8388_DACCONTROL3, &reg_val);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 将 DACMute (Bit 2) 置 1 */
    reg_val |= 0x04U;
    return dev_es8388_write_reg(dev, ES8388_DACCONTROL3, reg_val);
}

/**
 * @brief 关闭静音
 */
bsp_status_t dev_es8388_unmute(dev_es8388_t *dev)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    uint8_t reg_val = 0;
    bsp_status_t ret = dev_es8388_read_reg(dev, ES8388_DACCONTROL3, &reg_val);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 将 DACMute (Bit 2) 清 0 */
    reg_val &= ~0x04U;
    return dev_es8388_write_reg(dev, ES8388_DACCONTROL3, reg_val);
}
