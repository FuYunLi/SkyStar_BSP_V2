/**
 * @file dev_es8388.c
 * @brief ES8388 音频编解码器驱动实现源文件
 * @note  经 port_i2c（PORT_I2C_1，器件地址 0x10，CE=0）配置寄存器。
 *        初始化/启停/音量序列参照 RT-Thread 官方 stm32f407-rt-spark
 *        drv_es8388（Zero-Free，Apache-2.0），codec 为 I2S 从机模式。
 */

#define LOG_TAG "DEV_ES8388"

#include "dev_es8388.h"
#include "port_i2c.h"
#include "bsp_logger.h"

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

/* 器件地址：CE 引脚接地（CE=0）时为 0x10 */
#define ES8388_I2C_ID       (PORT_I2C_1)
#define ES8388_DEV_ADDR     (0x10U)
#define ES8388_I2C_TIMEOUT  (100U)

/* 寄存器地址合法性哨兵 */
#define ES8388_REG_MAX      (ES8388_DACCONTROL30)

/* 数字音量寄存器（DACCONTROL4/5）档位范围：0x00（0dB）~ 0xC0（-96dB） */
#define ES8388_VOL_MAX_REG  (0xC0U)

/* ================================================================
 * 私有辅助函数
 * ================================================================ */

/**
 * @brief 写单个寄存器
 */
static bsp_status_t es8388_reg_write(uint8_t reg, uint8_t val)
{
    return port_i2c_mem_write(ES8388_I2C_ID, ES8388_DEV_ADDR, reg,
                              1, &val, 1,
                              ES8388_I2C_TIMEOUT);
}

/**
 * @brief 读单个寄存器
 */
static bsp_status_t es8388_reg_read(uint8_t reg, uint8_t *val)
{
    return port_i2c_mem_read(ES8388_I2C_ID, ES8388_DEV_ADDR, reg,
                             1, val, 1,
                             ES8388_I2C_TIMEOUT);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

bsp_status_t dev_es8388_init(void)
{
    uint8_t val = 0;

    /* 上电序列（参照 RT-Thread 官方实现，注释保留原始语义） */
    (void)es8388_reg_write(ES8388_DACCONTROL3, 0x04);   /* DAC 静音，关闭软斜坡 */
    (void)es8388_reg_write(ES8388_CONTROL2, 0x50);      /* 模拟偏置与 VREF 配置 */
    (void)es8388_reg_write(ES8388_CHIPPOWER, 0x00);     /* 全部正常供电 */
    (void)es8388_reg_write(ES8388_MASTERMODE, 0x00);    /* codec 为 I2S 从机 */

    /* DAC 通路：先关闭输出再配置，防上电爆音 */
    (void)es8388_reg_write(ES8388_DACPOWER, 0xC0);      /* 关闭 DAC 与 LOUT/ROUT */
    (void)es8388_reg_write(ES8388_CONTROL1, 0x12);      /* 播放+录音模式，Enfr=0 */
    (void)es8388_reg_write(ES8388_DACCONTROL1, 0x18);   /* 16-bit I2S 格式 */
    (void)es8388_reg_write(ES8388_DACCONTROL2, 0x02);   /* 单速模式，FsRatio=256 */
    (void)es8388_reg_write(ES8388_DACCONTROL16, 0x00);  /* 混音源选择 LIN1/RIN1 */
    (void)es8388_reg_write(ES8388_DACCONTROL17, 0x9C);  /* 左 DAC 至左混音 0dB */
    (void)es8388_reg_write(ES8388_DACCONTROL20, 0x9C);  /* 右 DAC 至右混音 0dB */
    (void)es8388_reg_write(ES8388_DACCONTROL21, 0x80);  /* ADC/DAC 同 LRCK */
    (void)es8388_reg_write(ES8388_DACCONTROL23, 0x00);  /* VROI=0 */
    (void)es8388_reg_write(ES8388_DACCONTROL4, 0x00);   /* 数字音量 0dB */
    (void)es8388_reg_write(ES8388_DACCONTROL5, 0x00);
    (void)es8388_reg_write(ES8388_DACPOWER, 0x3C);      /* 使能 DAC 与 LOUT/ROUT */

    /* ADC 通路（录音通道，本阶段仅上电保持默认） */
    (void)es8388_reg_write(ES8388_ADCPOWER, 0xFF);      /* 先下电 */
    (void)es8388_reg_write(ES8388_ADCCONTROL1, 0xBB);   /* 麦克风 PGA 增益 */
    (void)es8388_reg_write(ES8388_ADCCONTROL2, 0x00);   /* LIN1/RIN1 输入 */
    (void)es8388_reg_write(ES8388_ADCCONTROL3, 0x02);
    (void)es8388_reg_write(ES8388_ADCCONTROL4, 0x0D);   /* I2S 格式与位长 */
    (void)es8388_reg_write(ES8388_ADCCONTROL5, 0x02);   /* 单速模式，FsRatio=256 */
    (void)es8388_reg_write(ES8388_ADCPOWER, 0x09);      /* ADC 上电 */

    /* LOUT1/ROUT1 输出音量：0x1E 平衡噪声档（官方推荐） */
    (void)es8388_reg_write(ES8388_DACCONTROL24, 0x1E);
    (void)es8388_reg_write(ES8388_DACCONTROL25, 0x1E);

    /* 自检：读回 CHIPPOWER 确认 I2C 通路与芯片响应 */
    if (es8388_reg_read(ES8388_CHIPPOWER, &val) != BSP_OK)
    {
        log_e("ES8388 I2C probe failed");
        return BSP_ENODEV;
    }
    log_i("ES8388 initialized, CHIPPOWER=0x%02X", val);
    return BSP_OK;
}

bsp_status_t dev_es8388_start_dac(void)
{
    uint8_t prev = 0, cur = 0;

    /* 启动状态机：CHIPPOWER 0xF0 → 0x00（官方序列，使能内部时钟链） */
    if (es8388_reg_read(ES8388_DACCONTROL21, &prev) != BSP_OK)
    {
        return BSP_EIO;
    }
    if (es8388_reg_write(ES8388_DACCONTROL21, 0x80) != BSP_OK)  /* 使能 DAC */
    {
        return BSP_EIO;
    }
    if (es8388_reg_read(ES8388_DACCONTROL21, &cur) != BSP_OK)
    {
        return BSP_EIO;
    }
    if (prev != cur)
    {
        if (es8388_reg_write(ES8388_CHIPPOWER, 0xF0) != BSP_OK ||
            es8388_reg_write(ES8388_CHIPPOWER, 0x00) != BSP_OK)
        {
            return BSP_EIO;
        }
    }
    if (es8388_reg_write(ES8388_DACPOWER, 0x3C) != BSP_OK)      /* 功放输出开启 */
    {
        return BSP_EIO;
    }
    /* 解除静音 */
    uint8_t mute = 0;
    if (es8388_reg_read(ES8388_DACCONTROL3, &mute) != BSP_OK)
    {
        return BSP_EIO;
    }
    return es8388_reg_write(ES8388_DACCONTROL3, (uint8_t)(mute & 0xFB));
}

bsp_status_t dev_es8388_stop_dac(void)
{
    bsp_status_t status = es8388_reg_write(ES8388_DACPOWER, 0x00);
    if (status != BSP_OK)
    {
        return status;
    }
    /* 置位 DACCONTROL3 bit2 数字静音 */
    uint8_t mute = 0;
    if (es8388_reg_read(ES8388_DACCONTROL3, &mute) != BSP_OK)
    {
        return BSP_EIO;
    }
    return es8388_reg_write(ES8388_DACCONTROL3, (uint8_t)(mute | 0x04));
}

bsp_status_t dev_es8388_set_dac_volume(uint8_t vol_0_100)
{
    if (vol_0_100 > 100U)
    {
        return BSP_EINVAL;
    }
    /* 官方映射：0-100 → 192*(100-vol)/100，0=0dB，100=-192 档（静音级） */
    uint8_t reg_val = (uint8_t)((192U * (100U - vol_0_100)) / 100U);
    bsp_status_t status = es8388_reg_write(ES8388_DACCONTROL4, reg_val);
    if (status != BSP_OK)
    {
        return status;
    }
    return es8388_reg_write(ES8388_DACCONTROL5, reg_val);
}

bsp_status_t dev_es8388_get_dac_volume(uint8_t *vol_0_100)
{
    if (vol_0_100 == NULL)
    {
        return BSP_EINVAL;
    }
    uint8_t reg_val = 0;
    bsp_status_t status = es8388_reg_read(ES8388_DACCONTROL4, &reg_val);
    if (status != BSP_OK)
    {
        return status;
    }
    if (reg_val >= ES8388_VOL_MAX_REG)
    {
        *vol_0_100 = 0;
    }
    else
    {
        *vol_0_100 = (uint8_t)(100U - (reg_val * 100U) / 192U);
    }
    return BSP_OK;
}

bsp_status_t dev_es8388_set_mute(bool en)
{
    uint8_t mute = 0;
    if (es8388_reg_read(ES8388_DACCONTROL3, &mute) != BSP_OK)
    {
        return BSP_EIO;
    }
    mute = en ? (uint8_t)(mute | 0x04) : (uint8_t)(mute & 0xFB);
    return es8388_reg_write(ES8388_DACCONTROL3, mute);
}

bsp_status_t dev_es8388_read_reg(uint8_t reg, uint8_t *val)
{
    if (val == NULL || reg > ES8388_REG_MAX)
    {
        return BSP_EINVAL;
    }
    return es8388_reg_read(reg, val);
}
