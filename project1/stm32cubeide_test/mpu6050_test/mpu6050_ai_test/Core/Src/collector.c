/**
 * @file    collector.c
 * @brief   数据采集器 + NanoEdge AI 异常检测（第三步）
 *
 * 工作方式：
 *   - TIM2 每 1ms 产生一次中断，Collector_OnTick() 只置标志位（不在中断里做 I2C 读写）
 *   - 主循环 Collector_Process() 看到标志位后读取 MPU6050，保证采样节拍精确
 *   - 命令 'r' 录制一个窗口（1024 样本 x 3 轴）；'s' 实时流；'c' 取消；'h' 帮助
 *   - 命令 'l' 用最近录制的窗口学习（喂给 NanoEdge 库，可重复录制后多次学习）
 *   - 命令 'd' 切换连续检测：自动录制实时窗口 -> detect() -> 打印相似度，低于阈值告警
 *
 * 注意：提示文字全部使用 ASCII；本文件需在加入 libneai.a / NanoEdgeAI.h 后编译。
 */
#include "collector.h"
#include "mpu6050.h"
#include "NanoEdgeAI.h"
#include "stdio.h"
#include "string.h"

/* 相似度低于该值视为异常（ST 推荐 80，可按实测灵敏度调整） */
#define ALARM_THRESHOLD     80

static TIM_HandleTypeDef  *tim;
static UART_HandleTypeDef *uart;
static I2C_HandleTypeDef  *i2c;

static volatile uint8_t tick_flag = 0;      /* 1ms 时基标志 */
static int16_t buf[COLLECT_SAMPLES][COLLECT_AXES];  /* 窗口缓冲：1024x3x2B = 6KB */
static volatile uint16_t idx = 0;
static volatile uint8_t recording = 0;      /* 正在录制 */
static volatile uint8_t window_ready = 0;   /* 窗口采集完成，待处理 */
static volatile uint8_t have_window = 0;    /* 有可用的录制窗口（供学习） */
static volatile uint8_t streaming = 0;      /* 实时流开关 */
static volatile uint8_t detecting = 0;      /* 连续检测开关 */
static uint16_t stream_div = 0;             /* 1kHz -> 100Hz 节流计数 */
static MPU6050_Accel_t a;

/* NanoEdge 输入缓冲：长度由生成的库决定（NEAI_INPUT_SIGNAL_LENGTH x 轴数） */
static float fbuf[NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER];

static void send_str(const char *s)
{
    HAL_UART_Transmit(uart, (uint8_t *)s, strlen(s), 100);
}

/* 把窗口里的一个子窗口（sig 个样本 x 3 轴）从 int16 转成 float 填入 fbuf */
static void fill_fbuf(uint16_t start)
{
    for (uint16_t k = 0; k < NEAI_INPUT_SIGNAL_LENGTH; k++) {
        fbuf[k * 3 + 0] = (float)buf[start + k][0];
        fbuf[k * 3 + 1] = (float)buf[start + k][1];
        fbuf[k * 3 + 2] = (float)buf[start + k][2];
    }
}

/* 学习：把 1024 样本窗口切成库要求的子窗口，逐个 learn() */
static void neai_learn_window(void)
{
    char tmp[48];
    if (!have_window) {
        send_str("[AI] no recorded window, press r first\r\n");
        return;
    }
    if (NEAI_INPUT_SIGNAL_LENGTH > COLLECT_SAMPLES) {
        send_str("[AI] signal length > window, reduce in Studio\r\n");
        return;
    }
    uint16_t start = 0;
    uint16_t n = 0;
    while (start + NEAI_INPUT_SIGNAL_LENGTH <= COLLECT_SAMPLES) {
        fill_fbuf(start);
        uint8_t st = neai_anomalydetection_learn(fbuf);
        n++;
        start += NEAI_INPUT_SIGNAL_LENGTH;
        if (st == NEAI_ERROR || st == NEAI_INVALID_PARAM) {
            break;
        }
    }
    sprintf(tmp, "[AI] learned %d signal(s)\r\n", n);
    send_str(tmp);
    send_str("[AI] record more windows and repeat l for richer knowledge\r\n");
}

/* 检测：对最近一个实时窗口做一次 detect()，打印相似度并按阈值告警 */
static void neai_detect_window(void)
{
    char tmp[32];
    uint8_t sim = 0;
    if (NEAI_INPUT_SIGNAL_LENGTH > COLLECT_SAMPLES) {
        send_str("[AI] signal length > window\r\n");
        return;
    }
    fill_fbuf(0);
    uint8_t st = neai_anomalydetection_detect(fbuf, &sim);
    if (st == NEAI_NOT_INITIALIZED) {
        send_str("[AI] not initialized\r\n");
        return;
    }
    sprintf(tmp, "SIM:%u\r\n", sim);
    send_str(tmp);
    if (sim < ALARM_THRESHOLD) {
        send_str("[ALARM] anomaly detected!\r\n");
        for (int j = 0; j < 3; j++) {           /* PC13 LED 闪 3 次提示 */
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            HAL_Delay(100);
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_Delay(100);
        }
    }
}

void Collector_Init(TIM_HandleTypeDef *htim, UART_HandleTypeDef *huart, I2C_HandleTypeDef *hi2c)
{
    tim = htim;
    uart = huart;
    i2c = hi2c;

    HAL_TIM_Base_Start_IT(tim);

    /* 板载 LED 默认熄灭（PC13 在 CubeMX 中设为 GPIO_Output） */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

    send_str("\r\n=== MPU6050 AI Detector v3 ===\r\n");
    send_str("Cmd: s=stream r=record l=learn d=detect c=cancel h=help\r\n");

    /* NanoEdge 库初始化：不用预训练知识，选择板上现场学习 */
    if (neai_anomalydetection_init(false) == NEAI_OK) {
        send_str("[AI] NanoEdge init OK, signal len=");
        sprintf((char *)fbuf, "%d\r\n", NEAI_INPUT_SIGNAL_LENGTH);  /* 复用缓冲区打印 */
        send_str((char *)fbuf);
    } else {
        send_str("[AI] NanoEdge init FAIL\r\n");
    }
}

/* TIM2 中断回调：只置标志，不做耗时操作 */
void Collector_OnTick(void)
{
    tick_flag = 1;
}

void Collector_HandleCmd(uint8_t cmd)
{
    switch (cmd) {
        case 'h':
        case 'H':
            send_str("[Help] s:stream  r:record  l:learn  d:detect  c:cancel  h:help\r\n");
            break;

        case 's':
        case 'S':
            streaming = !streaming;
            send_str(streaming ? "[Stream] started\r\n" : "[Stream] stopped\r\n");
            break;

        case 'r':
        case 'R':
            if (recording || detecting) {
                send_str("[Rec] busy, wait...\r\n");
                break;
            }
            idx = 0;
            recording = 1;
            have_window = 0;
            window_ready = 0;
            send_str("[Rec] start recording 1024 samples...\r\n");
            break;

        case 'l':
        case 'L':
            neai_learn_window();
            break;

        case 'd':
        case 'D':
            detecting = !detecting;
            recording = 0;      /* 关检测时同时取消在录的窗口，避免事后输出整块 CSV */
            window_ready = 0;
            send_str(detecting ? "[AI] detect mode ON\r\n" : "[AI] detect mode OFF\r\n");
            break;

        case 'c':
        case 'C':
            recording = 0;
            window_ready = 0;
            send_str("[Rec] cancelled\r\n");
            break;

        default:
            send_str("[?] unknown cmd, type h for help\r\n");
            break;
    }
}

void Collector_Process(void)
{
    if (!tick_flag) {
        return;
    }
    tick_flag = 0;

    /* 检测模式：空闲时自动开始录下一个实时窗口 */
    if (detecting && !recording && !window_ready) {
        idx = 0;
        recording = 1;
        have_window = 0;
    }

    /* 模式 1：录制窗口（不打印，只存缓冲） */
    if (recording) {
        if (MPU6050_Read_Accel(i2c, &a)) {
            buf[idx][0] = a.x;
            buf[idx][1] = a.y;
            buf[idx][2] = a.z;
            idx++;
            if (idx >= COLLECT_SAMPLES) {
                recording = 0;
                window_ready = 1;
                have_window = 1;
            }
        }
        return;
    }

    /* 模式 2：窗口已满，检测模式 -> 推理；否则按 CSV 块输出 */
    if (window_ready) {
        window_ready = 0;
        if (detecting) {
            neai_detect_window();
        } else {
            char line[32];
            send_str("====BEGIN====\r\n");
            send_str("x,y,z\r\n");
            for (uint16_t i = 0; i < COLLECT_SAMPLES; i++) {
                sprintf(line, "%d,%d,%d\r\n", buf[i][0], buf[i][1], buf[i][2]);
                HAL_UART_Transmit(uart, (uint8_t *)line, strlen(line), 100);
            }
            send_str("====END====\r\n");
            send_str("[Rec] done, copy 1024 data lines between BEGIN/END (skip x,y,z)\r\n");
        }
        return;
    }

    /* 模式 3：实时流（1kHz 节流到 100Hz 打印） */
    if (streaming) {
        stream_div++;
        if (stream_div >= 10) {
            stream_div = 0;
            if (MPU6050_Read_Accel(i2c, &a)) {
                char line[32];
                sprintf(line, "AX:%d AY:%d AZ:%d\r\n", a.x, a.y, a.z);
                HAL_UART_Transmit(uart, (uint8_t *)line, strlen(line), 100);
            }
        }
    }
}
