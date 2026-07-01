/**
 * @file dev_icm42688.c
 * @brief ICM-42688-P 六轴姿态传感器底层设备驱动实现源文件
 * @note 封装 SPI2 寄存器级读写，配置加速度计和陀螺仪工作参数，转换原始数据为物理单位。
 */

#include "dev_icm42688.h"
#include "port_spi.h"
#include "port_gpio.h"
#include "port_tick.h"

/* ================================================================
 * 寄存器定义
 * ================================================================ */

#define REG_DEVICE_CONFIG   (0x11U)  /* 设备配置寄存器 */
#define REG_DRIVE_CONFIG    (0x13U)  /* 驱动强度配置寄存器 */
#define REG_INT_CONFIG      (0x14U)  /* 中断配置寄存器 */
#define REG_FIFO_CONFIG     (0x16U)  /* FIFO配置寄存器 */
#define REG_TEMP_DATA1      (0x1DU)  /* 温度数据高字节寄存器 */
#define REG_ACCEL_DATA_X1   (0x1FU)  /* 加速度X轴高字节寄存器 */
#define REG_GYRO_DATA_X1    (0x25U)  /* 陀螺仪X轴高字节寄存器 */
#define REG_PWR_MGMT0       (0x4EU)  /* 电源管理0寄存器 */
#define REG_GYRO_CONFIG0    (0x4FU)  /* 陀螺仪配置0寄存器 */
#define REG_ACCEL_CONFIG0   (0x50U)  /* 加速度计配置0寄存器 */
#define REG_WHO_AM_I        (0x75U)  /* 器件ID寄存器 */
#define REG_BANK_SEL        (0x76U)  /* 寄存器Bank选择寄存器 */

/* ================================================================
 * 传感器参数常量
 * ================================================================ */

#define EXPECTED_WHO_AM_I   (0x47U)  /* ICM-42688-P 预期器件ID */

/* ================================================================
 * 私有函数声明与实现
 * ================================================================ */

/**
 * @brief 连续读取若干个寄存器的数据
 * @param reg 寄存器起始地址
 * @param data 存储读取数据的缓冲区指针
 * @param len 连续读取的字节长度
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
static bsp_status_t icm42688_read_regs(uint8_t reg, uint8_t *data, uint16_t len)
{
    uint8_t addr = reg | 0x80;
    port_gpio_write(PORT_GPIO_IMU_CS, PORT_GPIO_LOW);
    
    bsp_status_t status = port_spi_write(PORT_SPI_2, &addr, 1, 100);
    if (status == BSP_OK)
    {
        status = port_spi_read(PORT_SPI_2, data, len, 100);
    }
    
    port_gpio_write(PORT_GPIO_IMU_CS, PORT_GPIO_HIGH);
    return status;
}

/**
 * @brief 写入单个寄存器数据
 * @param reg 寄存器地址
 * @param val 要写入的数据
 * @return bsp_status_t 执行结果
 *         - BSP_OK 成功
 *         - 其他 失败
 */
static bsp_status_t icm42688_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx_buf[2] = { reg & 0x7F, val };
    port_gpio_write(PORT_GPIO_IMU_CS, PORT_GPIO_LOW);
    
    bsp_status_t status = port_spi_write(PORT_SPI_2, tx_buf, 2, 100);
    
    port_gpio_write(PORT_GPIO_IMU_CS, PORT_GPIO_HIGH);
    return status;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化姿态传感器
 */
bsp_status_t icm42688_init(void)
{
    bsp_status_t status = port_spi_init(PORT_SPI_2);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 默认拉高片选，保证总线空闲 */
    port_gpio_write(PORT_GPIO_IMU_CS, PORT_GPIO_HIGH);
    port_tick_delay_ms(10);

    /* 1. 软件复位传感器 */
    status = icm42688_write_reg(REG_DEVICE_CONFIG, 0x01);
    if (status != BSP_OK)
    {
        return status;
    }
    port_tick_delay_ms(15); /* 复位后需等待至少 10ms */

    /* 2. 强行锁定到 Bank 0 */
    status = icm42688_write_reg(REG_BANK_SEL, 0x00);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 3. 读取并校验 WHO_AM_I ID */
    uint8_t chip_id = 0;
    status = icm42688_read_regs(REG_WHO_AM_I, &chip_id, 1);
    if (status != BSP_OK)
    {
        return status;
    }
    
    if (chip_id != EXPECTED_WHO_AM_I)
    {
        return BSP_ENODEV;
    }

    /* 4. 配置量程与 ODR (±16g / ±2000 dps / 100 Hz) */
    /* GYRO_CONFIG0 (0x4F): Bits[7:5]=000 (±2000dps), Bits[3:0]=1000 (100Hz ODR) */
    status = icm42688_write_reg(REG_GYRO_CONFIG0, 0x08);
    if (status != BSP_OK)
    {
        return status;
    }

    /* ACCEL_CONFIG0 (0x50): Bits[7:5]=000 (±16g), Bits[3:0]=1000 (100Hz ODR) */
    status = icm42688_write_reg(REG_ACCEL_CONFIG0, 0x08);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 5. 开启传感器 (加速度计与陀螺仪均设为 LN 模式) */
    /* PWR_MGMT0 (0x4E): Bits[3:2]=11 (Gyro LN Mode), Bits[1:0]=11 (Accel LN Mode) */
    status = icm42688_write_reg(REG_PWR_MGMT0, 0x0F);
    if (status != BSP_OK)
    {
        return status;
    }

    /* LN 模式开启后，陀螺仪稳定需要 45ms+，此处延时 50ms */
    port_tick_delay_ms(50);
    
    return BSP_OK;
}

/**
 * @brief 读取最新的 6 轴加速度计、陀螺仪与芯片温度数据并转换为物理量
 */
bsp_status_t icm42688_read_data(icm42688_data_t *data)
{
    if (data == NULL)
    {
        return BSP_EINVAL;
    }

    uint8_t rx_buf[14] = {0};
    
    /* 连续读取温度、3轴加速度、3轴陀螺仪共 14 字节寄存器空间 (0x1D ~ 0x2A) */
    bsp_status_t status = icm42688_read_regs(REG_TEMP_DATA1, rx_buf, 14);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 原始 16 位补码数据转换 */
    int16_t temp_raw  = (int16_t)((rx_buf[0]  << 8) | rx_buf[1]);
    int16_t accel_x   = (int16_t)((rx_buf[2]  << 8) | rx_buf[3]);
    int16_t accel_y   = (int16_t)((rx_buf[4]  << 8) | rx_buf[5]);
    int16_t accel_z   = (int16_t)((rx_buf[6]  << 8) | rx_buf[7]);
    int16_t gyro_x    = (int16_t)((rx_buf[8]  << 8) | rx_buf[9]);
    int16_t gyro_y    = (int16_t)((rx_buf[10] << 8) | rx_buf[11]);
    int16_t gyro_z    = (int16_t)((rx_buf[12] << 8) | rx_buf[13]);

    /* 物理单位换算 */
    /* 1. 温度 (℃) = (temp_raw / 132.48) + 25.0 */
    data->temp_c = ((float)temp_raw / 132.48f) + 25.0f;

    /* 2. 加速度 (g)：在 ±16g 量程下，LSB 灵敏度为 32768 / 16 = 2048 LSB/g */
    data->accel_x_g = (float)accel_x / 2048.0f;
    data->accel_y_g = (float)accel_y / 2048.0f;
    data->accel_z_g = (float)accel_z / 2048.0f;

    /* 3. 角速度 (dps)：在 ±2000 dps 量程下，LSB 灵敏度为 32768 / 2000 = 16.4 LSB/dps */
    data->gyro_x_dps = (float)gyro_x / 16.384f;
    data->gyro_y_dps = (float)gyro_y / 16.384f;
    data->gyro_z_dps = (float)gyro_z / 16.384f;

    return BSP_OK;
}


