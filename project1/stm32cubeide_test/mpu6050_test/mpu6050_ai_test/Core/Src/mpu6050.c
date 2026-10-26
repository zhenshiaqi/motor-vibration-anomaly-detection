/**
 * @file    mpu6050.c
 * @brief   GY-521 (MPU6050) 三轴加速度计驱动实现
 * @note    所有读写均走 HAL 硬件 I2C（hi2c1）。
 *
 * v4 改动（都是为了"烧完没反应"这类现场问题好查）：
 *   1. 所有 I2C 操作使用 MPU6050_I2C_TIMEOUT_MS 有限超时，不再 HAL_MAX_DELAY；
 *   2. 初始化先软复位，再读 WHO_AM_I，读不到就明确返回失败（而不是假装成功）；
 *   3. 提供 MPU6050_BusRecover()：从机把 SDA 拉死时手动打时钟解锁总线。
 *
 * 注意：加速度量程（±2g）、DLPF（默认关闭）、SMPLRT_DIV（0）必须保持与原版一致，
 *       否则已训练的 NanoEdge 模型看到的信号分布会变，相似度阈值要重标定。
 */
#include "mpu6050.h"
#include "i2c.h"

#define MPU6050_REG_VALUE_DEFAULT   0x00

static uint8_t who_am_i = 0;   /* 最近一次初始化读到的芯片 ID */

/* ---- 内部小工具：带超时的单寄存器写 / 读，成功返回 0 ---- */
static uint8_t reg_write(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t val)
{
    return (HAL_I2C_Mem_Write(hi2c, MPU6050_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                              &val, 1, MPU6050_I2C_TIMEOUT_MS) == HAL_OK) ? 0U : 1U;
}

static uint8_t reg_read(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *val)
{
    return (HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             val, 1, MPU6050_I2C_TIMEOUT_MS) == HAL_OK) ? 0U : 1U;
}

uint8_t MPU6050_GetWhoAmI(void)
{
    return who_am_i;
}

uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id = 0;

    who_am_i = 0;

    /* 0) 软复位：清掉上一次运行（或半途复位）留下的寄存器状态。
     *    PWR_MGMT_1 bit7 = DEVICE_RESET，写完要等芯片内部重启。 */
    if (reg_write(hi2c, MPU6050_REG_PWR_MGMT_1, 0x80) != 0U) {
        return 0;   /* 连复位都写不进去：I2C 不通（接线/供电/地址） */
    }
    HAL_Delay(100);

    /* 1) 唤醒芯片：PWR_MGMT_1 写 0x00，清掉 SLEEP 位，时钟源用内部 8MHz */
    if (reg_write(hi2c, MPU6050_REG_PWR_MGMT_1, MPU6050_REG_VALUE_DEFAULT) != 0U) {
        return 0;
    }
    HAL_Delay(10);

    /* 2) 读 WHO_AM_I：确认总线上真的有芯片、且地址对
     *    0x68 = MPU6050；0x70~0x74 = MPU6500/MPU9250 等兼容芯片（寄存器兼容，照用） */
    if (reg_read(hi2c, MPU6050_REG_WHO_AM_I, &id) != 0U) {
        return 0;
    }
    who_am_i = id;
    if (id != 0x68U && (id < 0x70U || id > 0x74U)) {
        return 0;   /* 有应答但 ID 不对：多半是别的 I2C 器件占了 0x68 */
    }

    /* 3) 加速度计量程 ±2g（寄存器默认就是 0，写上是为了显式可读） */
    if (reg_write(hi2c, MPU6050_REG_ACCEL_CONFIG, MPU6050_REG_VALUE_DEFAULT) != 0U) {
        return 0;
    }

    /* 4) 陀螺仪量程 ±250°/s（本项目用不到，保持默认） */
    (void)reg_write(hi2c, MPU6050_REG_GYRO_CONFIG, MPU6050_REG_VALUE_DEFAULT);

    /* 5) 采样率分频：SMPLRT_DIV = 0 -> 采样率 1kHz（为振动分析预留） */
    (void)reg_write(hi2c, MPU6050_REG_SMPLRT_DIV, MPU6050_REG_VALUE_DEFAULT);

    return 1;
}

uint8_t MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, MPU6050_Accel_t *accel)
{
    uint8_t buf[6];

    /* 从 0x3B 起连续读 6 字节：AX_H AX_L AY_H AY_L AZ_H AZ_L */
    if (HAL_I2C_Mem_Read(hi2c, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buf, 6,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK) {
        return 0;
    }

    /* 高字节在前，低字节在后，拼成 int16 */
    accel->x = (int16_t)((buf[0] << 8) | buf[1]);
    accel->y = (int16_t)((buf[2] << 8) | buf[3]);
    accel->z = (int16_t)((buf[4] << 8) | buf[5]);

    return 1;
}

uint8_t MPU6050_BusRecover(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t i;

    /* 释放硬件 I2C，把 PB6/PB7 拿回来当普通 GPIO 用 */
    HAL_I2C_DeInit(hi2c);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin   = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;   /* 开漏，和 I2C 电气一致 */
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* 两线先都放高（空闲态） */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    /* 打 9 个 SCL 时钟：把卡在"等 ACK"状态的从机移位寄存器顶出去 */
    for (i = 0; i < 9U; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        for (volatile uint32_t d = 0; d < 200U; d++) { __NOP(); }
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < 200U; d++) { __NOP(); }
    }

    /* 手动发一个 STOP：SCL 高电平期间 SDA 由低变高 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    /* 把 PB6/PB7 交回 I2C1，重新初始化外设 */
    MX_I2C1_Init();
    HAL_Delay(1);

    return 1;
}
