/**
 * @file dev_sd3078.c
 * @brief SD3078 RTC 驱动实现
 * @note 遵循 Allman 风格及 BSP 架构规范
 */

#include "dev_sd3078.h"

/* 硬件映射配置 (根据板级原理图配置) */
#define SD3078_I2C_BUS PORT_I2C_1
#define SD3078_INT_PIN PORT_GPIO_SD3078_INT

/* 寄存器地址定义 */
#define SD3078_REG_TIME_START  0x00 /* 00H-06H: 秒,分,时,周,日,月,年 */
#define SD3078_REG_ALARM_START 0x07 /* 07H-0DH: 报警对应位 */
#define SD3078_REG_ALARM_EN    0x0E /* 0EH: 报警允许寄存器 */
#define SD3078_REG_CTR1        0x0F /* 0FH: 控制寄存器 1 (含 WRTC2/3, INTAF/DF) */
#define SD3078_REG_CTR2        0x10 /* 10H: 控制寄存器 2 (含 WRTC1, IM, INTS, INTDE/AE) */
#define SD3078_REG_CTR3        0x11 /* 11H: 控制寄存器 3 (含 TDS) */
#define SD3078_REG_TIMER_START 0x13 /* 13H~15H: 24位倒数定时器起始 */
#define SD3078_REG_TEMP        0x16 /* 16H: 温度寄存器 */
#define SD3078_REG_CHRG        0x18 /* 18H: 充电寄存器 */
#define SD3078_REG_BAT         0x1A /* 1AH: 电池电量/欠压标志 */

/* 宏定义位操作与配置 */
#define SD3078_CHRG_DISABLE    0x00 /* 禁止充电 */
#define SD3078_CTR2_WRTC1      0x80 /* 写允许位 1 (10H.D7) */
#define SD3078_CTR1_WRTC2      0x04 /* 写允许位 2 (0FH.D2) */
#define SD3078_CTR1_WRTC3      0x80 /* 写允许位 3 (0FH.D7) */
#define SD3078_CTR1_UNLOCK_VAL 0xFF /* 官方建议写开启赋 FFH */
#define SD3078_CTR1_LOCK_VAL   0x7B /* 官方建议写禁止赋 7BH */

#define SD3078_BAT_LOW_VOLT_MASK 0x01 /* 1AH.D0: 欠压标志位 BLF */

#define SD3078_CTR1_INTAF 0x20 /* 0FH.D5: 报警中断标志位 */
#define SD3078_CTR1_INTDF 0x10 /* 0FH.D4: 倒计时中断标志位 */

#define SD3078_CTR2_IM    0x40 /* 10H.D6: 中断模式位 (1:Level, 0:Pulse) */
#define SD3078_CTR2_INTS1 0x20 /* 10H.D5: 中断输出选通 1 */
#define SD3078_CTR2_INTS0 0x10 /* 10H.D4: 中断输出选通 0 */
#define SD3078_CTR2_INTDE 0x04 /* 10H.D2: 倒计时中断允许位 */
#define SD3078_CTR2_INTAE 0x02 /* 10H.D1: 报警中断允许位 */

#define SD3078_CTR3_TDS_MASK  0x30 /* 11H.D5-D4: 倒计时频率选择 (TDS) */
#define SD3078_CTR3_TDS_SHIFT 4

/* 时间寄存器掩码 */
#define SD3078_MASK_SEC   0x7F
#define SD3078_MASK_MIN   0x7F
#define SD3078_MASK_WEEK  0x07
#define SD3078_MASK_DAY   0x3F
#define SD3078_MASK_MONTH 0x1F

/* 超时时间 */
#define SD3078_TIMEOUT_MS 100

/**
 * @brief BCD 码转十进制
 */
static uint8_t bcd_to_dec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }

/**
 * @brief 十进制转 BCD 码
 */
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/**
 * @brief SD3078 写允许解锁
 */
static bsp_status_t sd3078_write_enable(void)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    /* 第一步：WRTC1=1 (10H 第7位) */
    val    = SD3078_CTR2_WRTC1;
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR2, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 第二步：WRTC2=1, WRTC3=1 (0FH 官方建议赋 FFH 以解锁并保持标志位不被干扰) */
    val    = SD3078_CTR1_UNLOCK_VAL;
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR1, 1, &val, 1, SD3078_TIMEOUT_MS);

    return status;
}

/**
 * @brief SD3078 写允许闭锁
 */
static bsp_status_t sd3078_write_disable(void)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    /* 第一步：WRTC2/3=0 (0FH 官方建议赋 7BH) */
    val    = SD3078_CTR1_LOCK_VAL;
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR1, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 第二步：控制寄存器 2 (10H) WRTC1=0 */
    val    = 0x00;
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR2, 1, &val, 1, SD3078_TIMEOUT_MS);

    return status;
}

/**
 * @brief 关闭 SD3078 内部充电电路 (防爆保护)
 */
bsp_status_t dev_sd3078_disable_charging(void)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    /* 1. 解锁写保护 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 禁止充电 (18H 宏地址写入语义值) */
    val    = SD3078_CHRG_DISABLE;
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CHRG, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status != BSP_OK)
    {
        (void)sd3078_write_disable();
        return status;
    }

    /* 3. 闭锁写保护 */
    status = sd3078_write_disable();

    return status;
}

/**
 * @brief SD3078 基础初始化
 */
bsp_status_t dev_sd3078_init(void)
{
    bsp_status_t status = BSP_OK;

    /* 初始化 I2C 总线 */
    status = port_i2c_init(SD3078_I2C_BUS);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 强制执行防爆保护：关闭内部充电 */
    status = dev_sd3078_disable_charging();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 物理级清场：强制扫除过去的遗留中断，释放 INT 引脚高电平，避免卡死复位后无法捕捉下降沿 */
    dev_sd3078_clear_countdown_flag();
    dev_sd3078_clear_alarm_flag();

    return BSP_OK;
}

/**
 * @brief 读取 SD3078 内部温度
 *
 * @param temperature 存放读取到的温度值指针（摄氏度）
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_get_temperature(int8_t *temperature)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    if (temperature == NULL)
    {
        return BSP_EINVAL;
    }

    status = port_i2c_mem_read(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_TEMP, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status == BSP_OK)
    {
        *temperature = (int8_t)val;
    }

    return status;
}

/**
 * @brief 获取电池电量/欠压状态
 *
 * @param is_low_voltage 存放欠压标志的指针（true 代表欠压）
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_get_battery_status(bool *is_low_voltage)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    if (is_low_voltage == NULL)
    {
        return BSP_EINVAL;
    }

    status = port_i2c_mem_read(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_BAT, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status == BSP_OK)
    {
        /* 1AH.D0 为电池欠压标志位 BLF */
        *is_low_voltage = (val & SD3078_BAT_LOW_VOLT_MASK) ? true : false;
    }

    return status;
}

/**
 * @brief 从 SD3078 读取当前时间
 *
 * @param time 存放读取到的时间结构体指针
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_get_time(sd3078_time_t *time)
{
    uint8_t      buf[7] = { 0 };
    bsp_status_t status = BSP_OK;

    if (time == NULL)
    {
        return BSP_EINVAL;
    }

    /* 连读 00H~06H */
    status = port_i2c_mem_read(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_TIME_START, 1, buf, 7, SD3078_TIMEOUT_MS);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 解析并剥离掩位 */
    time->second  = bcd_to_dec(buf[0] & SD3078_MASK_SEC);
    time->minute  = bcd_to_dec(buf[1] & SD3078_MASK_MIN);
    time->hour    = bcd_to_dec(buf[2] & SD3078_HOUR_VALUE_MASK_24H);
    time->weekday = bcd_to_dec(buf[3] & SD3078_MASK_WEEK);
    time->day     = bcd_to_dec(buf[4] & SD3078_MASK_DAY);
    time->month   = bcd_to_dec(buf[5] & SD3078_MASK_MONTH);
    time->year    = bcd_to_dec(buf[6]);

    return status;
}

/**
 * @brief 设置 SD3078 当前时间
 *
 * @param time 待设置的时间结构体指针
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_set_time(const sd3078_time_t *time)
{
    uint8_t      buf[7] = { 0 };
    bsp_status_t status = BSP_OK;

    if (time == NULL)
    {
        return BSP_EINVAL;
    }

    /* 转换十进制到 BCD */
    buf[0] = dec_to_bcd(time->second);
    buf[1] = dec_to_bcd(time->minute);
    buf[2] = dec_to_bcd(time->hour) | SD3078_HOUR_MODE_24H; /* 施加 24H 模式控制 */
    buf[3] = dec_to_bcd(time->weekday);
    buf[4] = dec_to_bcd(time->day);
    buf[5] = dec_to_bcd(time->month);
    buf[6] = dec_to_bcd(time->year);

    /* 1. 解开写保护 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 连写时间寄存器 00H~06H */
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_TIME_START, 1, buf, 7, SD3078_TIMEOUT_MS);

    /* 3. 无论写入成功与否，均尝试闭锁 */
    (void)sd3078_write_disable();

    return status;
}

/**
 * @brief 设置 SD3078 报警时间及模式
 *
 * @param alarm_time 报警时间结构体指针
 * @param mask_flags 报警掩码（定义哪些位参与比较，见 sd3078_alarm_mask_t）
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_set_alarm(const sd3078_time_t *alarm_time, uint8_t mask_flags)
{
    uint8_t      buf[7] = { 0 };
    bsp_status_t status = BSP_OK;

    if (alarm_time == NULL)
    {
        return BSP_EINVAL;
    }

    /* 转换报警时间到 BCD (07H~0DH 对应 00H~06H) */
    buf[0] = dec_to_bcd(alarm_time->second);
    buf[1] = dec_to_bcd(alarm_time->minute);
    buf[2] = dec_to_bcd(alarm_time->hour) | SD3078_HOUR_MODE_24H; /* 报警小时也需匹配 24H 模式标志 */
    buf[3] = dec_to_bcd(alarm_time->weekday);
    buf[4] = dec_to_bcd(alarm_time->day);
    buf[5] = dec_to_bcd(alarm_time->month);
    buf[6] = dec_to_bcd(alarm_time->year);

    /* 1. 解锁 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 连写报警值寄存器 07H~0DH */
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_ALARM_START, 1, buf, 7, SD3078_TIMEOUT_MS);
    if (status == BSP_OK)
    {
        /* 3. 设置报警允许寄存器 0EH */
        status =
            port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_ALARM_EN, 1, (uint8_t *)&mask_flags, 1, SD3078_TIMEOUT_MS);
        if (status == BSP_OK)
        {
            /* 4. 配置 10H (CTR2) 开启报警中断允许 INTAE 并设置路由到引脚 INTS=01 */
            uint8_t ctr2_val = SD3078_CTR2_WRTC1 | SD3078_CTR2_INTS0 | SD3078_CTR2_INTAE;
            status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR2, 1, &ctr2_val, 1, SD3078_TIMEOUT_MS);
        }
    }

    /* 5. 闭锁 */
    (void)sd3078_write_disable();

    return status;
}

/**
 * @brief 清除 SD3078 报警中断标志
 *
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_clear_alarm_flag(void)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    /* 1. 解锁 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 读取控制寄存器 1 (0FH) */
    status = port_i2c_mem_read(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR1, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status == BSP_OK)
    {
        /* 3. 清理 D5 (INTAF) */
        val &= ~SD3078_CTR1_INTAF;
        /* 回写 0FH。注意：此时仍处于解锁状态，建议保持其他位为 1 (0xFF) 或显式回写 */
        status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR1, 1, &val, 1, SD3078_TIMEOUT_MS);
    }

    /* 4. 闭锁 */
    (void)sd3078_write_disable();

    return status;
}

/**
 * @brief 设置 SD3078 倒计时功能
 *
 * @param init_val 倒计时初始值 (0~255)
 * @param freq 倒计时频率 (见 sd3078_timer_freq_t)
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_set_countdown(uint8_t init_val, sd3078_timer_freq_t freq)
{
    uint8_t      val[3] = { 0 };
    bsp_status_t status = BSP_OK;

    /* 1. 解锁 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 复位倒计时配置 (需配置路由 INTS1=1, INTS0=1 使能引脚输出) */
    val[0] = SD3078_CTR2_WRTC1 | SD3078_CTR2_INTS1 | SD3078_CTR2_INTS0;
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR2, 1, &val[0], 1, SD3078_TIMEOUT_MS);

    if (status == BSP_OK)
    {
        /* 3. 配置 11H (CTR3) 的频率 TDS */
        val[0] = (freq << SD3078_CTR3_TDS_SHIFT) & SD3078_CTR3_TDS_MASK;
        status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR3, 1, &val[0], 1, SD3078_TIMEOUT_MS);
    }

    if (status == BSP_OK)
    {
        /* 4. 写入 24-bit 倒数初始值到 13H~15H (目前仅使用低8位) */
        val[0] = init_val;
        val[1] = 0x00;
        val[2] = 0x00;
        status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_TIMER_START, 1, val, 3, SD3078_TIMEOUT_MS);
    }

    if (status == BSP_OK)
    {
        /* 5. 启动倒计时使能 (INTDE=1) */
        val[0] = SD3078_CTR2_WRTC1 | SD3078_CTR2_INTS1 | SD3078_CTR2_INTS0 | SD3078_CTR2_INTDE;
        status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR2, 1, &val[0], 1, SD3078_TIMEOUT_MS);
    }

    /* 6. 闭锁 */
    (void)sd3078_write_disable();

    return status;
}

/**
 * @brief 清除 SD3078 倒计时中断标志
 *
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_clear_countdown_flag(void)
{
    uint8_t      val    = 0;
    bsp_status_t status = BSP_OK;

    /* 1. 解锁 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 读取控制寄存器 1 (0FH) */
    status = port_i2c_mem_read(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR1, 1, &val, 1, SD3078_TIMEOUT_MS);
    if (status == BSP_OK)
    {
        /* 3. 清理 D4 (INTDF) */
        val &= ~SD3078_CTR1_INTDF;
        /* 回写 0FH。保持解锁开启位以防标志清理失效，建议回写 */
        status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_CTR1, 1, &val, 1, SD3078_TIMEOUT_MS);
    }

    /* 4. 闭锁 */
    (void)sd3078_write_disable();

    return status;
}

/**
 * @brief 从 SD3078 用户 SRAM 读取数据
 * @note SRAM 地址范围 2CH~71H，共 70 字节
 *
 * @param offset SRAM 内部偏移量 (0~69)
 * @param data 存放读取数据的缓冲区指针
 * @param len 待读取长度
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_read_sram(uint8_t offset, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    /* 边界检查：防止 offset + len 超过最大 SRAM 范围 */
    if ((uint16_t)offset + (uint16_t)len > SD3078_SRAM_SIZE_MAX)
    {
        return BSP_EINVAL;
    }

    return port_i2c_mem_read(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_SRAM_START + offset, 1, data, len, SD3078_TIMEOUT_MS);
}

/**
 * @brief 向 SD3078 用户 SRAM 写入数据
 * @note SRAM 地址范围 2CH~71H，共 70 字节。写入操作需解除写保护。
 *
 * @param offset SRAM 内部偏移量 (0~69)
 * @param data 待写入数据的缓冲区指针
 * @param len 待写入长度
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_write_sram(uint8_t offset, uint8_t *data, uint8_t len)
{
    bsp_status_t status = BSP_OK;

    if (data == NULL || len == 0)
    {
        return BSP_EINVAL;
    }

    if ((uint16_t)offset + (uint16_t)len > SD3078_SRAM_SIZE_MAX)
    {
        return BSP_EINVAL;
    }

    /* 1. 解锁写保护 */
    status = sd3078_write_enable();
    if (status != BSP_OK)
    {
        return status;
    }

    /* 2. 写入 SRAM 数据 */
    status = port_i2c_mem_write(SD3078_I2C_BUS, SD3078_I2C_ADDR_8BIT, SD3078_REG_SRAM_START + offset, 1,
                                (uint8_t *)data, len, SD3078_TIMEOUT_MS);

    /* 3. 闭锁写保护 */
    (void)sd3078_write_disable();

    return status;
}

/**
 * @brief 挂载 SD3078 外部中断回调
 * @note 用于接收报警 (Alarm) 或倒计时 (Timer) 中断信号
 *
 * @param cb 外部中断回调函数指针
 * @return bsp_status_t
 */
bsp_status_t dev_sd3078_int_attach(port_exti_callback_t cb)
{
    if (cb == NULL)
    {
        return BSP_EINVAL;
    }

    /* SD3078 INT 引脚发生中断时拉低，对应下降沿触发 */
    return port_gpio_exti_init(SD3078_INT_PIN, PORT_EXTI_TRIGGER_FALLING, cb);
}
