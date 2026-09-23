/**
 * @file bsp_imu.c
 * @brief 板级姿态传感器 (ICM-42688-P) 服务实现源文件
 * @note  封装 SPI2 选通（经 bsp_bus 总线仲裁器）、轮询更新及互补滤波计算。
 */

#define LOG_TAG "IMU_SRV"

#include "bsp_imu.h"
#include "bsp_bus.h"
#include "dev_icm42688.h"
#include "port_critical.h"
#include "bsp_logger.h"
#include <math.h>

/* ================================================================
 * 宏定义与常量
 * ================================================================ */

#define M_PI_F           (3.1415926f)

/* ================================================================
 * 私有静态变量
 * ================================================================ */

static bsp_imu_raw_t s_raw_data = {0};
static bsp_imu_attitude_t s_attitude = {0};
static bool s_is_init = false;
static bool s_attitude_inited = false;
/* 总线被 I2S2 占用期间挂起 SPI 采样，恢复后无需重配 ICM-42688 寄存器 */
static volatile bool s_suspended = false;

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化 IMU 板级支持服务
 */
bsp_status_t bsp_imu_init(void)
{
    /* 1. 经总线仲裁器申请 SPI2 归属（含 PCA9555 模拟开关选通） */
    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);
    if (status != BSP_OK)
    {
        log_e("SPI2 bus acquire failed");
        return status;
    }
    log_i("SPI2 bus channel locked via bus arbiter");

    /* 物理侧已就绪，立即释放占用权：IMU 采样通过挂起标志参与仲裁，
     * 不长期持有 claim，避免阻塞 I2S2 侧的音频接管 */
    (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);

    /* 2. 调用底层驱动进行设备初始化与检查 */
    status = icm42688_init();
    if (status != BSP_OK)
    {
        /* 必须带上状态码：-7=器件不答/ID 不匹配、-4=超时、-1=SPI 事务失败，
         * 三者对应完全不同的排查方向，丢码会把定位带成猜谜 */
        log_e("ICM-42688-P physical hardware init failed, status = %d", (int)status);
        return status;
    }

    s_is_init = true;
    s_attitude_inited = false;
    log_i("IMU service module registered successfully");
    return BSP_OK;
}

/**
 * @brief 姿态解算周期更新任务 (周期恒定 dt = 10ms = 0.01s)
 */
bsp_status_t bsp_imu_update(void)
{
    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    /* 总线被 I2S2 归属期间 SPI2 不可用，静默挂起本轮采样 */
    if (s_suspended)
    {
        return BSP_BUSY;
    }

    icm42688_data_t dev_data = {0};
    bsp_status_t status = icm42688_read_data(&dev_data);
    if (status != BSP_OK)
    {
        return status;
    }

    /* 互补滤波预处理：计算瞬时倾角 */
    /* 1. Roll (横滚角)：绕 X 轴旋转角度，由 Y 和 Z 轴加速度计算 */
    float accel_roll = atan2f(dev_data.accel_y_g, dev_data.accel_z_g) * (180.0f / M_PI_F);

    /* 2. Pitch (俯仰角)：绕 Y 轴旋转角度，由 X 轴与 Z、Y 矢量合力计算 */
    float denom = sqrtf(dev_data.accel_y_g * dev_data.accel_y_g + dev_data.accel_z_g * dev_data.accel_z_g);
    if (denom < 0.0001f)
    {
        denom = 0.0001f; // 防0除截断保护
    }
    float accel_pitch = atan2f(-dev_data.accel_x_g, denom) * (180.0f / M_PI_F);

    /* 3. 临界区安全更新全局读数与角度数据，防竞态脏读 */
    uint32_t primask = port_enter_critical();

    s_raw_data.accel_x = dev_data.accel_x_g;
    s_raw_data.accel_y = dev_data.accel_y_g;
    s_raw_data.accel_z = dev_data.accel_z_g;
    s_raw_data.gyro_x  = dev_data.gyro_x_dps;
    s_raw_data.gyro_y  = dev_data.gyro_y_dps;
    s_raw_data.gyro_z  = dev_data.gyro_z_dps;
    s_raw_data.temp    = dev_data.temp_c;

    if (!s_attitude_inited)
    {
        /* 第一次采样直接由加速度计获取绝对初始姿态，防积分缓慢爬升 */
        s_attitude.roll  = accel_roll;
        s_attitude.pitch = accel_pitch;
        s_attitude_inited = true;
    }
    else
    {
        /* 一阶互补滤波公式，比例为 96% 陀螺仪积分 + 4% 加速度计倾角纠正，dt = 0.01s */
        s_attitude.roll  = 0.96f * (s_attitude.roll  + dev_data.gyro_x_dps * 0.01f) + 0.04f * accel_roll;
        s_attitude.pitch = 0.96f * (s_attitude.pitch + dev_data.gyro_y_dps * 0.01f) + 0.04f * accel_pitch;
    }

    port_exit_critical(primask);

    return BSP_OK;
}

/**
 * @brief 获取最新采样并换算后的六轴物理原始读数
 */
bsp_status_t bsp_imu_get_raw(bsp_imu_raw_t *raw)
{
    if (raw == NULL)
    {
        return BSP_EINVAL;
    }
    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    uint32_t primask = port_enter_critical();
    *raw = s_raw_data;
    port_exit_critical(primask);

    return BSP_OK;
}

/**
 * @brief 获取最新的俯仰角和横滚角姿态信息
 */
bsp_status_t bsp_imu_get_attitude(bsp_imu_attitude_t *att)
{
    if (att == NULL)
    {
        return BSP_EINVAL;
    }
    if (!s_is_init)
    {
        return BSP_ERROR;
    }

    uint32_t primask = port_enter_critical();
    *att = s_attitude;
    port_exit_critical(primask);

    return BSP_OK;
}

/**
 * @brief 挂起 IMU 采样（供 bsp_bus 仲裁器切换至 I2S2 侧时调用）
 */
bsp_status_t bsp_imu_suspend(void)
{
    if (!s_is_init)
    {
        return BSP_ERROR;
    }
    s_suspended = true;
    log_i("IMU sampling suspended for bus handover");
    return BSP_OK;
}

/**
 * @brief 恢复 IMU 采样（供 bsp_bus 仲裁器切回 SPI2 侧时调用）
 */
bsp_status_t bsp_imu_resume(void)
{
    if (!s_is_init)
    {
        return BSP_ERROR;
    }
    s_suspended = false;
    log_i("IMU sampling resumed");
    return BSP_OK;
}

