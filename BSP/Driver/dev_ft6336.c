/**
 * @file dev_ft6336.c
 * @brief FT6336 电容触摸芯片底层物理驱动实现
 */

#include "dev_ft6336.h"
#include <stddef.h>

/* FT6336 内部寄存器偏置地址定义 */
#define FT6336_REG_TD_STATUS       0x02U /* 触摸状态和点数寄存器 */
#define FT6336_REG_CHIP_VENDOR_ID  0xA3U /* 芯片厂商ID寄存器 */
#define FT6336_REG_FIRMWARE_ID     0xA6U /* 固件版本号寄存器 */

/* 预期的芯片参数 */
#define FT6336_EXPECTED_VENDOR_ID  0x64U /* 预期的厂商ID (0x64) */
#define FT6336_I2C_TIMEOUT_MS      50U   /* I2C 读写超时时间 (ms) */

/* 实例化板级默认触摸设备句柄 */
ft6336_dev_t touch_dev = 
{
    .i2c_bus = PORT_I2C_SOFT_1
};

/* =========================================================================
 * 内部私有辅助函数
 * ========================================================================= */

/**
 * @brief 从芯片内部指定寄存器连续读取数据
 * @param dev 设备句柄指针
 * @param reg_addr 起始寄存器偏置地址
 * @param buf 接收缓冲区指针
 * @param len 待读取的字节长度
 * @retval BSP_OK 读取成功
 * @retval 其他 错误码
 */
static bsp_status_t ft6336_read_regs(ft6336_dev_t *dev, uint8_t reg_addr, uint8_t *buf, uint16_t len)
{
    return port_i2c_mem_read(dev->i2c_bus, FT6336_I2C_ADDR_8BIT, reg_addr, 1, buf, len, FT6336_I2C_TIMEOUT_MS);
}

/* =========================================================================
 * 导出 API 函数实现
 * ========================================================================= */

/**
 * @brief 初始化 FT6336 电容触摸屏设备
 * @param dev FT6336 设备句柄指针
 * @retval BSP_OK 初始化成功且验证通过
 * @retval BSP_EINVAL 参数指针非法
 * @retval BSP_ENODEV 芯片未响应或厂商 ID 不匹配
 * @note 根据 FT6336 手册规范，在初始化时通过 I2C 读回厂商 ID 寄存器（0xA3）
 *       以验证物理芯片的连通性与正确性。
 * @example
 * dev_ft6336_init(&touch_dev);
 */
bsp_status_t dev_ft6336_init(ft6336_dev_t *dev)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    dev->touch_count = 0;
    for (int i = 0; i < FT6336_MAX_TOUCH_POINTS; ++i)
    {
        dev->points[i].valid = false;
    }

    /* 初始化底层关联的软件 I2C 控制引脚 */
    bsp_status_t ret = port_i2c_init(dev->i2c_bus);
    if (ret != BSP_OK)
    {
        return ret;
    }

    /* 读取芯片厂商 ID 寄存器进行硬件在线校验 */
    uint8_t vendor_id = 0;
    ret = ft6336_read_regs(dev, FT6336_REG_CHIP_VENDOR_ID, &vendor_id, 1);
    if (ret != BSP_OK)
    {
        return BSP_ERROR;
    }

    if (vendor_id != FT6336_EXPECTED_VENDOR_ID)
    {
        return BSP_ENODEV;
    }

    return BSP_OK;
}

/**
 * @brief 读取并解析 FT6336 的触摸状态及坐标数据
 * @param dev FT6336 设备句柄指针
 * @retval BSP_OK 读取并解析成功
 * @retval BSP_EINVAL 参数指针非法
 * @retval BSP_ERROR 硬件总线传输异常
 * @note 硬件原理与寄存器手册映射：
 *       1. 读取从寄存器 0x02 (TD_STATUS) 起始的 13 字节寄存器组。
 *       2. 依据寄存器映射关系，解析 0x02 的低 4 位获取当前有效触摸点数。
 *       3. 解析单点/双点数据：
 *          - X 坐标：高位取 0x03 (P1_XH) 的低 4 位，低位取 0x04 (P1_XL) 的 8 位合成 12 位坐标值。
 *          - Y 坐标：高位取 0x05 (P1_YH) 的低 4 位，低位取 0x06 (P1_YL) 的 8 位合成 12 位坐标值。
 *          - 事件标志：取 P1_XH 寄存器的最高 2 位（bit 7 ~ 6），分别对应按下、抬起、接触或无事件。
 *          - 触摸 ID：取 P1_YH 寄存器的最高 4 位（bit 7 ~ 4）作为追踪指纹 ID。
 */
bsp_status_t dev_ft6336_read_touch(ft6336_dev_t *dev)
{
    if (dev == NULL)
    {
        return BSP_EINVAL;
    }

    /* 寄存器缓冲区：TD_STATUS (1 字节) + 最多 2 个点的触摸数据 (每点 6 字节) */
    uint8_t data_buf[1 + FT6336_MAX_TOUCH_POINTS * 6] = {0};
    
    bsp_status_t ret = ft6336_read_regs(dev, FT6336_REG_TD_STATUS, data_buf, sizeof(data_buf));
    if (ret != BSP_OK)
    {
        return BSP_ERROR;
    }

    /* 解析当前发生触摸的有效手指数量 (TD_STATUS[3:0]) */
    dev->touch_count = data_buf[0] & 0x0FU;
    if (dev->touch_count > FT6336_MAX_TOUCH_POINTS)
    {
        dev->touch_count = FT6336_MAX_TOUCH_POINTS;
    }

    /* 清除上一周期的无效点标志 */
    for (int i = 0; i < FT6336_MAX_TOUCH_POINTS; ++i)
    {
        dev->points[i].valid = false;
    }

    /* 遍历并解析各个有效触摸点的信息 */
    for (uint8_t i = 0; i < dev->touch_count; ++i)
    {
        /* 定位到缓冲区中当前点的起始数据段 (每点占用 6 字节) */
        uint8_t *point_data = &data_buf[1 + i * 6];
        ft6336_touch_point_t *point = &dev->points[i];
        
        point->valid = true;

        /* 解析事件标志 (位于 Px_XH 的 bit 7 ~ 6) */
        point->event = (ft6336_touch_event_t)((point_data[0] >> 6) & 0x03U);

        /* 解析 X 坐标 (高 4 位位于 Px_XH[3:0]，低 8 位位于 Px_XL) */
        point->x = (uint16_t)(((uint16_t)(point_data[0] & 0x0FU) << 8) | point_data[1]);

        /* 解析追踪 ID (位于 Px_YH 的 bit 7 ~ 4) */
        point->touch_id = (point_data[2] >> 4) & 0x0FU;

        /* 解析 Y 坐标 (高 4 位位于 Px_YH[3:0]，低 8 位位于 Px_YL) */
        point->y = (uint16_t)(((uint16_t)(point_data[2] & 0x0FU) << 8) | point_data[3]);
    }

    return BSP_OK;
}
