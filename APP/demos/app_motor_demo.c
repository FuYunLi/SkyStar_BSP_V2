/**
 * @file app_motor_demo.c
 * @brief 直流电机控制自检演示实现
 * @note 对标 RocketPi 18_rocketpi_pwm_motor。【未验证】外部电机尚未
 *       接线实测，控制时序以 AT8236 数据手册真值表为准。
 */

#define LOG_TAG "APP_MOTOR"

#include "app_motor_demo.h"
#include "bsp_logger.h"
#include "dev_motor.h"
#include "shell.h"
#include <stdlib.h>

/* ================================================================
 * 私有函数
 * ================================================================ */

/**
 * @brief motor Shell 指令入口：设置速度与方向
 */
static int shell_motor(int argc, char *argv[])
{
    if (argc != 2)
    {
        log_i("usage: motor <-1000..1000>  (positive=CW, negative=CCW)");
        return -1;
    }

    int speed = atoi(argv[1]);
    bsp_status_t ret = dev_motor_set_speed(DEV_MOTOR_1, (int16_t)speed);
    if (ret != BSP_OK)
    {
        log_e("motor set failed! ret = %d", ret);
        return -1;
    }

    log_i("Motor speed = %d permille.", speed);
    return 0;
}

/**
 * @brief motor_ramp Shell 指令入口：-1000→1000→-1000 缓加速扫掠
 */
static int shell_motor_ramp(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    log_i("Motor ramp start...");

    /* 正转加速 */
    for (int16_t speed = 0; speed <= 1000; speed += 100)
    {
        (void)dev_motor_set_speed(DEV_MOTOR_1, speed);
        bsp_tick_delay_ms(100);
    }

    /* 反转加速 */
    for (int16_t speed = 1000; speed >= -1000; speed -= 100)
    {
        (void)dev_motor_set_speed(DEV_MOTOR_1, speed);
        bsp_tick_delay_ms(100);
    }

    /* 回零 */
    for (int16_t speed = -1000; speed <= 0; speed += 100)
    {
        (void)dev_motor_set_speed(DEV_MOTOR_1, speed);
        bsp_tick_delay_ms(100);
    }

    (void)dev_motor_stop(DEV_MOTOR_1);
    log_i("Motor ramp done.");
    return 0;
}

/**
 * @brief motor_stop Shell 指令入口：滑行停止
 */
static int shell_motor_stop(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bsp_status_t ret = dev_motor_stop(DEV_MOTOR_1);
    if (ret != BSP_OK)
    {
        log_e("motor stop failed! ret = %d", ret);
        return -1;
    }

    log_i("Motor coasting.");
    return 0;
}

/**
 * @brief motor_brake Shell 指令入口：刹车
 */
static int shell_motor_brake(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bsp_status_t ret = dev_motor_brake(DEV_MOTOR_1);
    if (ret != BSP_OK)
    {
        log_e("motor brake failed! ret = %d", ret);
        return -1;
    }

    log_i("Motor braked.");
    return 0;
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * @brief 初始化直流电机控制演示模块
 */
bsp_status_t app_motor_demo_init(void)
{
    bsp_status_t ret = dev_motor_init(DEV_MOTOR_1);
    if (ret != BSP_OK)
    {
        log_e("Motor init failed! ret = %d", ret);
        return ret;
    }

    log_i("Motor Demo loaded. Try: motor <speed> / motor_stop / motor_brake");
    return BSP_OK;
}

/* ================================================================
 * Shell 指令导出声明
 * ================================================================ */

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, motor, shell_motor, Set motor speed permille (-1000..1000));
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, motor_ramp, shell_motor_ramp, Ramp motor speed both directions);
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, motor_stop, shell_motor_stop, Coast motor (IN1=IN2=0));
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, motor_brake, shell_motor_brake, Brake motor (IN1=IN2=1));
