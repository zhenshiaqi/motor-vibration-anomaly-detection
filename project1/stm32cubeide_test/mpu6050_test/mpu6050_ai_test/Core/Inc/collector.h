/**
 * @file    collector.h
 * @brief   数据采集器 + NanoEdge AI 异常检测集成（第三步）
 *
 * 功能：
 *   s  实时流（100Hz）        r  录制一个窗口（1024 样本 x 3 轴）
 *   l  用最近录制的窗口学习    d  切换连续检测模式（实时窗口 -> 相似度 -> 告警）
 *   c  取消录制               h  帮助
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

void Collector_Init(TIM_HandleTypeDef *htim, UART_HandleTypeDef *huart, I2C_HandleTypeDef *hi2c);
void Collector_OnTick(void);
void Collector_HandleCmd(uint8_t cmd);
void Collector_Process(void);

#endif /* __COLLECTOR_H */
