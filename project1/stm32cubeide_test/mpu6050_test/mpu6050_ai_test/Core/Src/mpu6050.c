/**
 * @file    mpu6050.c
 * @brief   GY-521 (MPU6050) 三轴加速度计驱动实现
 * @note    所有读写均走 HAL 硬件 I2C（hi2c1）。
 *          若 HAL 硬件 I2C 偶发卡死/超时（F103 的已知小坑），
 *          可改用软件模拟 I2C，到时候再给你替代版本。
 */
#include "mpu6050.h"

uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t data;

    /* 1) 唤醒芯片：PWR_MGMT_1 写 0x00，清掉 SLEEP 位 */
    data = 0x00;
    if (HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_REG_PWR_MGMT_1,
                          I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK) {
        return 0;
    }

    /* 2) 加速度计量程 ±2g（寄存器默认就是 0，写上是为了显式可读） */
    data = 0x00;
    if (HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_REG_ACCEL_CONFIG,
                          I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY) != HAL_OK) {
        return 0;
    }

    /* 3) 陀螺仪量程 ±250°/s（本项目用不到，保持默认） */
    data = 0x00;
    HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_REG_GYRO_CONFIG,
                      I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    /* 4) 采样率分频：SMPLRT_DIV = 0 -> 采样率 1kHz（为后面振动分析预留） */
    data = 0x00;
    HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, MPU6050_REG_SMPLRT_DIV,
                      I2C_MEMADD_SIZE_8BIT, &data, 1, HAL_MAX_DELAY);

    return 1;
}

uint8_t MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_Accel_t *accel)
{
    uint8_t buf[6];

    /* 从 0x3B 起连续读 6 字节：AX_H AX_L AY_H AY_L AZ_H AZ_L */
    if (HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buf, 6, HAL_MAX_DELAY) != HAL_OK) {
        return 0;
    }

    /* 高字节在前，低字节在后，拼成 int16 */
    accel->x = (int16_t)((buf[0] << 8) | buf[1]);
    accel->y = (int16_t)((buf[2] << 8) | buf[3]);
    accel->z = (int16_t)((buf[4] << 8) | buf[5]);

    return 1;
}
