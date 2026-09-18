/**
 * @file dev_es8388.c
 * @brief ES8388 音频编解码器驱动实现
 * @note 寄存器序列逐条对照立创官方出厂例程（SkyStar-fdb
 *       0_example/Factory/libraries/Board_Drivers/audio/drv_es8388.c，
 *       Apache-2.0）校准，去除 RT-Thread 依赖后移植到本 BSP 分层。
 *       I2C 地址约定：ES8388 数据手册 7 位地址 0x10（CE=0），
 *       port_i2c 使用 8 位形式 0x20。
 *       【未验证】音频链路尚未上板实测。
 */

#include "dev_es8388.h"
#include "port_i2c.h"
#include "port_dwt.h"

/* ================================================================
 * 私有宏定义
 * ================================================================ */

#define ES8388_ADDR8        (0x20U)  /* 7 位地址 0x10 左移 1 位 */
#define ES8388_I2C_TIMEOUT  (100U)   /* I2C 超时（ms） */
#define ES8388_BOOT_DELAY   (50U)    /* 上电序列间隔（ms），对照官方例程 */

/* 常用寄存器（完整映射见数据手册，仅列本驱动涉及的） */
#define ES8388_CONTROL1     (0x00U)  /* 芯片工作模式与参考电压 */
#define ES8388_CONTROL2     (0x01U)  /* 模拟偏置与 LP 相关 */
#define ES8388_CHIPPOWER    (0x02U)  /* 数字核电源 */
#define ES8388_ADCPOWER     (0x03U)  /* ADC 模拟块电源 */
#define ES8388_ADCCONTROL1  (0x09U)  /* MIC 放大器 PGA 增益 */
#define ES8388_ADCCONTROL2  (0x0AU)  /* 输入通道选择与差分配置 */
#define ES8388_DACPOWER     (0x04U)  /* DAC 与输出级电源 */
#define ES8388_MASTERMODE   (0x08U)  /* 主从模式选择 */
#define ES8388_DACCONTROL1  (0x17U)  /* DAC 串口格式（I2S/位宽） */
#define ES8388_DACCONTROL2  (0x18U)  /* DAC MCLK/LRCK 分频比 */
#define ES8388_DACCONTROL3  (0x19U)  /* DAC 静音与去加重 */
#define ES8388_DACCONTROL4  (0x1AU)  /* DAC 左声道音量 */
#define ES8388_DACCONTROL5  (0x1BU)  /* DAC 右声道音量 */
#define ES8388_DACCONTROL16 (0x26U)  /* 混音与 LOUT1 相关 */
#define ES8388_DACCONTROL17 (0x27U)  /* LOUT1 音量 */
#define ES8388_DACCONTROL20 (0x29U)  /* ROUT1 音量 */
#define ES8388_DACCONTROL21 (0x2AU)  /* 混音与 ROUT1 相关 */
#define ES8388_DACCONTROL23 (0x2CU)  /* 输出相关 */
#define ES8388_DACCONTROL24 (0x2DU)  /* LOUT2 音量 */
#define ES8388_DACCONTROL25 (0x2EU)  /* ROUT2 音量 */

/* 音量换算：0-100 → 芯片码值 192-0（0dB 为上限） */
#define ES8388_VOL_CODE_MAX (192U)

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief 写单个寄存器
 */
static bsp_status_t s_es8388_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};

    return port_i2c_write(PORT_I2C_1, ES8388_ADDR8, buf, sizeof(buf), ES8388_I2C_TIMEOUT);
}

/**
 * @brief 读单个寄存器（组合传输：写寄存器地址后重启读）
 */
static bsp_status_t s_es8388_read(uint8_t reg, uint8_t *val)
{
    if (val == NULL)
    {
        return BSP_EINVAL;
    }

    return port_i2c_mem_read(PORT_I2C_1, ES8388_ADDR8, reg, 1U, val, 1U, ES8388_I2C_TIMEOUT);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 按官方上电序列初始化 ES8388
 */
bsp_status_t dev_es8388_init(void)
{
    bsp_status_t ret = BSP_OK;

    /* 软复位：写 0x00 寄存器复位位后清零 */
    ret = s_es8388_write(ES8388_CONTROL1, 0x80U);
    if (ret != BSP_OK)
    {
        return ret;
    }
    port_dwt_delay_us(ES8388_BOOT_DELAY * 1000U);
    ret = s_es8388_write(ES8388_CONTROL1, 0x00U);
    if (ret != BSP_OK)
    {
        return ret;
    }
    port_dwt_delay_us(ES8388_BOOT_DELAY * 1000U);

    /* 先静音，防止上电爆音 */
    ret = s_es8388_write(ES8388_DACCONTROL3, 0x04U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* I2S 从机模式，数字核上电，ADC/DAC 模拟块暂不下电使能 */
    (void)s_es8388_write(ES8388_CONTROL2, 0x50U);
    (void)s_es8388_write(ES8388_CHIPPOWER, 0x00U);
    (void)s_es8388_write(ES8388_MASTERMODE, 0x00U);

    /* DAC 播放路径基础配置：I2S 16 位、MCLK/LRCK = 256fs 单速模式，
     * 初始化期间输出级下电（0xC0）避免噪声，启动时再上电 */
    (void)s_es8388_write(ES8388_DACPOWER, 0xC0U);
    (void)s_es8388_write(ES8388_CONTROL1, 0x12U);
    (void)s_es8388_write(ES8388_DACCONTROL1, 0x18U);
    (void)s_es8388_write(ES8388_DACCONTROL2, 0x02U);
    (void)s_es8388_write(ES8388_DACCONTROL16, 0x00U);
    (void)s_es8388_write(ES8388_DACCONTROL17, 0x9CU);
    (void)s_es8388_write(ES8388_DACCONTROL20, 0x9CU);
    (void)s_es8388_write(ES8388_DACCONTROL21, 0x80U);
    (void)s_es8388_write(ES8388_DACCONTROL23, 0x00U);

    /* 数字音量 0dB（满幅），输出级保持下电直至 start */
    (void)s_es8388_write(ES8388_DACCONTROL4, 0x00U);
    (void)s_es8388_write(ES8388_DACCONTROL5, 0x00U);
    (void)s_es8388_write(ES8388_DACPOWER, 0x00U);

    /* 输出 2（板载 HT6872 功放路径）默认中档音量 */
    (void)s_es8388_write(ES8388_DACCONTROL24, 0x1EU);
    (void)s_es8388_write(ES8388_DACCONTROL25, 0x1EU);

    /* 存活检查：读回工作模式寄存器 */
    uint8_t who = 0xFFU;
    ret = s_es8388_read(ES8388_CONTROL1, &who);
    if ((ret != BSP_OK) || (who == 0xFFU))
    {
        return BSP_ENODEV;
    }

    return BSP_OK;
}

/**
 * @brief 启动 DAC 播放路径
 */
bsp_status_t dev_es8388_start(void)
{
    /* 复位数字核电源确保重配置后的时钟被锁存，再上电输出级并解除静音 */
    bsp_status_t ret = s_es8388_write(ES8388_CHIPPOWER, 0xF0U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = s_es8388_write(ES8388_CHIPPOWER, 0x00U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* DAC 与模拟输出上电（LOUT1/LOUT2/ROP/RON） */
    ret = s_es8388_write(ES8388_DACPOWER, 0x3CU);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 清除静音位 */
    uint8_t mute_reg = 0U;
    ret = s_es8388_read(ES8388_DACCONTROL3, &mute_reg);
    if (ret != BSP_OK)
    {
        return ret;
    }
    mute_reg &= (uint8_t)~0x04U;

    return s_es8388_write(ES8388_DACCONTROL3, mute_reg);
}

/**
 * @brief 启动 ADC 录音路径
 */
bsp_status_t dev_es8388_start_adc(void)
{
    /* 复位数字核电源确保时钟锁存（与 start 一致） */
    bsp_status_t ret = s_es8388_write(ES8388_CHIPPOWER, 0xF0U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    ret = s_es8388_write(ES8388_CHIPPOWER, 0x00U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 板载麦克风为差分输入（对照官方例程注释），中档 PGA */
    (void)s_es8388_write(ES8388_ADCCONTROL1, 0x88U);
    (void)s_es8388_write(ES8388_ADCCONTROL2, 0xF0U);

    /* ADC 模拟块整体上电（含 MICBIAS） */
    return s_es8388_write(ES8388_ADCPOWER, 0x00U);
}

/**
 * @brief 停止 ADC 录音路径
 */
bsp_status_t dev_es8388_stop_adc(void)
{
    return s_es8388_write(ES8388_ADCPOWER, 0xFFU);
}

/**
 * @brief 停止 DAC 播放路径
 */
bsp_status_t dev_es8388_stop(void)
{
    bsp_status_t ret = s_es8388_write(ES8388_DACCONTROL3, 0x04U);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return s_es8388_write(ES8388_DACPOWER, 0x00U);
}

/**
 * @brief 设置 DAC 音量
 */
bsp_status_t dev_es8388_set_volume(uint8_t volume)
{
    if (volume > 100U)
    {
        volume = 100U;
    }

    /* 线性映射：音量 100 → 码值 0（0dB），音量 0 → 码值 192（静音） */
    uint8_t code = (uint8_t)((ES8388_VOL_CODE_MAX * (100U - volume)) / 100U);

    bsp_status_t ret = s_es8388_write(ES8388_DACCONTROL4, code);
    if (ret != BSP_OK)
    {
        return ret;
    }

    return s_es8388_write(ES8388_DACCONTROL5, code);
}
