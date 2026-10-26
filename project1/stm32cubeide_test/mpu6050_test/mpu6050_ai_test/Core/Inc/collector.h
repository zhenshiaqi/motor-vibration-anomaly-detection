/**
 * @file    collector.h
 * @brief   数据采集器 + NanoEdge AI 异常检测集成（第四版）
 *
 * 功能：
 *   s  实时流（100Hz）        r  录制一个窗口（1024 样本 x 3 轴）
 *   l  用最近录制的窗口学习    d  切换连续检测模式（实时窗口 -> 相似度 -> 告警）
 *   c  取消录制               h  帮助
 *
 * v4 相对 v3 的改动：
 *   1. Collector_Init() 增加 sensor_ok 参数：传感器没接好也照样跑，命令能回话；
 *   2. 告警灯改成非阻塞（100ms 状态机），不会把采样/串口卡住 600ms；
 *   3. 空闲时每 5 秒打印一行心跳，方便确认"固件在跑、串口在线"。
 *
 * 前提：已把 NanoEdge AI Studio 生成的 libneai.a / NanoEdgeAI.h 加入工程。
 * CubeMX 还需把 PC13（板载 LED）设为 GPIO_Output。
 */
#ifndef __COLLECTOR_H
#define __COLLECTOR_H

#include "main.h"

/* 采集参数：一个信号窗口 = COLLECT_SAMPLES 个样本（@1kHz 约 1 秒） */
#define COLLECT_SAMPLES     1024
#define COLLECT_AXES        3

/* 空闲心跳间隔（毫秒），设为 0 关闭 */
#define HEARTBEAT_MS        5000U

/* sensor_ok：MPU6050 是否初始化成功（1=在线，0=没接好）。
 * 传 0 时采集/检测命令只回错误提示，不会去碰 I2C。 */
void Collector_Init(TIM_HandleTypeDef *htim, UART_HandleTypeDef *huart, I2C_HandleTypeDef *hi2c,
                    uint8_t sensor_ok);
void Collector_OnTick(void);
void Collector_HandleCmd(uint8_t cmd);
void Collector_Process(void);

#endif /* __COLLECTOR_H */
