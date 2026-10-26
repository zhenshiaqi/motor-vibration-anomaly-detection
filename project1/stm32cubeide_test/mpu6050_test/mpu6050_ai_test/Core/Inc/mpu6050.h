/**
 * @file    mpu6050.h
 * @brief   GY-521 (MPU6050) 三轴加速度计驱动 —— 配合 STM32F103C8T6 (Blue Pill) 使用
 *          适用第一步：通过串口把振动/加速度数据读到电脑上位机。
 *
 * 硬件接线（Blue Pill 的 I2C1）：
 *   GY-521 VCC  -> 3.3V (或 5V，模块自带稳压)
 *   GY-521 GND  -> GND
 *   GY-521 SCL  -> PB6 (I2C1_SCL)
 *   GY-521 SDA  -> PB7 (I2C1_SDA)
 *
 * CubeMX 要求：
 *   1. SYS -> Debug = Serial Wire（否则烧录一次后无法再下载）
 *   2. I2C1 打开，Speed Mode = Fast Mode 400KHz（不稳就改 Standard 100KHz）
 *   3. USART1 打开，115200, 8N1
 */
#ifndef __MPU6050_H
#define __MPU6050_H

#include "main.h"   /* CubeMX 生成的工程主头文件，含 I2C_HandleTypeDef 等 */

/* GY-521 (MPU6050) 的 I2C 地址：
 *   AD0 引脚悬空 -> 0x68；AD0 接 VCC -> 0x69
 * HAL 库要求 8 位地址格式，所以左移 1 位 */
#define MPU6050_ADDR         (0x68 << 1)

/* 常用寄存器地址（用到的即可） */
#define MPU6050_REG_SMPLRT_DIV     0x19   /* 采样率分频 */
#define MPU6050_REG_CONFIG         0x1A   /* DLPF 滤波 */
#define MPU6050_REG_GYRO_CONFIG    0x1B   /* 陀螺仪量程（本项目用不到） */
#define MPU6050_REG_ACCEL_CONFIG   0x1C   /* 加速度计量程 */
#define MPU6050_REG_ACCEL_XOUT_H   0x3B   /* 加速度 X 高字节（连续读 6 字节） */
#define MPU6050_REG_PWR_MGMT_1     0x6B   /* 电源管理：唤醒芯片 / 软复位 */
#define MPU6050_REG_WHO_AM_I       0x75   /* 芯片 ID：MPU6050 应为 0x68 */

/* 单次 I2C 寄存器读写的超时（毫秒）。
 * 以前用 HAL_MAX_DELAY：总线被拉死时 HAL 会无限等待，整块板子就"假死"，
 * 串口一行字都出不来，非常难查。改成有限超时后一定能走到失败分支。 */
#define MPU6050_I2C_TIMEOUT_MS     50U

/* 加速度计 ±2g 量程下：1g = 16384 LSB */
#define MPU6050_ACCEL_LSB_PER_G    16384.0f

/* 三轴原始数据（int16） */
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} MPU6050_Accel_t;

/* 初始化：软复位、唤醒芯片、设置 ±2g 量程、采样率 1kHz，并读 WHO_AM_I 确认芯片在线。
 * 成功返回 1，失败返回 0（失败时串口能打印出是哪一步失败）。 */
uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c);

/* 读取三轴加速度原始值，成功返回 1，失败返回 0 */
uint8_t MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_Accel_t *accel);

/* 最近一次 MPU6050_Init 读到的 WHO_AM_I（0 表示没读到）。
 * 0x68 = MPU6050；0x70~0x74 = MPU6500/MPU9250 等兼容芯片。 */
uint8_t MPU6050_GetWhoAmI(void);

/* I2C 总线自救：从机把 SDA 拉死（掉电重启后常见）时，用 GPIO 手动打 9 个 SCL
 * 时钟 + 一个 STOP，再把 I2C1 重新初始化。成功返回 1。 */
uint8_t MPU6050_BusRecover(I2C_HandleTypeDef *hi2c);

#endif /* __MPU6050_H */
