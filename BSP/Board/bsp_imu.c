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

/* 初始化未成功时的重试退避：不能“失败一次就永久变哑”，也不能每秒刷日志 */
#define IMU_RETRY_INTERVAL_MS  (1000U)
#define IMU_RETRY_LOG_EVERY    (30U)   /* 首次与之后每 30 次（约 30s）报一条 */

/* ================================================================
 * 私有静态变量
 * ================================================================ */

static bsp_imu_raw_t s_raw_data = {0};
static bsp_imu_attitude_t s_attitude = {0};
static bool s_is_init = false;
static bool s_attitude_inited = false;
/* 挂起标志：音频持有 SPI2/I2S2 复用总线时的快速出口（不等于是总线安全的唯一保障，
 * 事务级的 bsp_bus_acquire 才是） */
static volatile bool s_suspended = false;
static uint32_t s_next_retry_ms;              /* 下次允许重试初始化的时刻 */
static uint32_t s_retry_count;                /* 累计重试次数，用于日志节流 */

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 硬件初始化的内部实现
 * @param verbose 0=后台重试调用，抑制重复日志；非 0=失败时逐条 log_e
 * @note 占用权必须始终持有到器件配置完成：旧写法是 acquire 完立即 release 再跑
 *       icm42688_init()，留下约 75ms 空窗；期间若音频把模拟开关切到 I2S2 侧，
 *       ICM42688 会收到“CS 有效但时钟缺失”的半截事务，配置不生效且不会报任何错
 */
static bsp_status_t s_imu_hw_init(uint8_t verbose)
{
    /* 1. 经总线仲裁器申请 SPI2 归属（含 PCA9555 模拟开关选通） */
    bsp_status_t status = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);
    if (status != BSP_OK)
    {
        if ((verbose != 0U) && (status != BSP_BUSY))
        {
            log_e("SPI2 bus acquire failed, status = %d", (int)status);
        }
        return status;
    }

    /* 2. 在占用权保护下完成设备初始化与检查 */
    status = icm42688_init();
    (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);

    if (status != BSP_OK)
    {
        /* 必须带上状态码：-7=器件不答/ID 不匹配、-4=超时、-1=SPI 事务失败，
         * 三者对应完全不同的排查方向，丢码会把定位带成猜谜 */
        if (verbose != 0U)
        {
            log_e("ICM-42688-P physical hardware init failed, status = %d", (int)status);
        }
        return status;
    }

    s_is_init = true;
    s_attitude_inited = false;
    log_i("IMU service module registered successfully");
    return BSP_OK;
}

/**
 * @brief 初始化 IMU 板级支持服务
 */
bsp_status_t bsp_imu_init(void)
{
    return s_imu_hw_init(1U);
}

/**
 * @brief 姿态解算周期更新任务 (周期恒定 dt = 10ms = 0.01s)
 */
bsp_status_t bsp_imu_update(void)
{
    /* 挂起标志是快速出口（不必去敲仲裁器）；真正的事务保护在下面那对 acquire/release */
    if (s_suspended)
    {
        return BSP_BUSY;
    }

    /* 未就绪则带退避重试：一次坏环境不得把 IMU 永久变哑，但也不能每秒刷日志 */
    if (!s_is_init)
    {
        uint32_t now = bsp_tick_get_ms();

        if ((int32_t)(now - s_next_retry_ms) < 0)
        {
            return BSP_ERROR;
        }
        s_next_retry_ms = now + IMU_RETRY_INTERVAL_MS;
        s_retry_count++;

        if ((s_retry_count == 1U) || ((s_retry_count % IMU_RETRY_LOG_EVERY) == 0U))
        {
            log_i("IMU not ready, retry #%lu (every %u ms)",
                  (unsigned long)s_retry_count, (unsigned)IMU_RETRY_INTERVAL_MS);
        }

        if (s_imu_hw_init(0U) != BSP_OK)
        {
            return BSP_ERROR;
        }

        log_i("IMU recovered after %lu attempt(s)", (unsigned long)s_retry_count);
    }

    /* 设备事务纳入仲裁：与 bsp_lfs 对称。拿不到占用权就直返 BUSY，
     * 绕不产生“mux 已切走但仍在拉 CS、打时钟”的半截事务 */
    bsp_status_t bus = bsp_bus_acquire(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);
    if (bus != BSP_OK)
    {
        return bus;
    }

    icm42688_data_t dev_data = {0};
    bsp_status_t status = icm42688_read_data(&dev_data);

    (void)bsp_bus_release(BSP_BUS_SPI2_I2S2, BSP_BUS_OWNER_SPI2);

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
 * @note 必须无条件置位。挂起的语义是“别碰这根总线”，与器件是否初始化成功无关。
 *       旧写法带 if (!s_is_init) return BSP_ERROR; —— 于是“初始化失败”恰好是唯一
 *       不能挂起的状态，形成自锁死：mux 切到 I2S2 后 10ms 定时器仍拉 CS 打时钟，
 *       芯片收到半截事务表现为读回全 0（实测 id=0x00），且此后 imu_read 永远 -1
 */
bsp_status_t bsp_imu_suspend(void)
{
    if (!s_suspended)
    {
        s_suspended = true;
        log_i("IMU sampling suspended for bus handover");
    }

    return BSP_OK;
}

/**
 * @brief 恢复 IMU 采样（供 bsp_bus 仲裁器切回 SPI2 侧时调用）
 */
bsp_status_t bsp_imu_resume(void)
{
    if (s_suspended)
    {
        s_suspended = false;
        log_i("IMU sampling resumed");
    }

    return BSP_OK;
}

