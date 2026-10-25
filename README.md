# 小型电机 振动异常检测（边缘 AI + 预测性维护）

用一颗 **STM32F103C8T6（Blue Pill）+ MPU6050** 把小型电机/风扇的振动采集到板上，再用 **ST NanoEdge AI Studio** 训练异常检测模型，把模型以静态库形式跑在单片机里——**1 kHz 采样、板上学习正常态、实时输出相似度并告警**，全程不联网、不依赖云端推理。

> 一句话：机械振动信号 → CSV 数据集 → NanoEdge AI 异常检测库 → 边缘端实时预测性维护。

---

## 1. 项目亮点

| 项目 | 实测值 / 说明 |
| --- | --- |
| Benchmark 主 KPI | **0.9934**（MML 算法，20570 次自动评估后选出） |
| 综合得分 | 0.9806 |
| 模型占用 | **RAM 624 B / Flash 564 B**（远低于 F103C8 的 20 KB / 64 KB） |
| 采样与窗口 | 1 kHz 固定频率，1024 样本 × 3 轴 ≈ 1 秒信号 |
| 工作方式 | **on-board training**：上电后在板上现场学习正常态，无需重训固件 |
| 告警 | 相似度低于阈值（默认 80）→ 串口 `[ALARM]` + 板载 LED（PC13）闪 3 次 |
| 成本 | 主控 ~10 元 + 传感器 ~5 元 + 下载器 ~15 元，全部离线 |

NanoEdge AI Studio 是 ST 官方免费工具（ST Edge AI Suite 的一部分），模型以 `libneai.a` + `NanoEdgeAI.h` 交付，直接链进 GCC 工程。

---

## 2. 系统架构

```
         ┌──────────────┐   I2C1 (400 kHz)   ┌──────────────────┐
 振动 ──▶│  GY-521      │───────────────────▶│  STM32F103C8T6   │
         │  MPU6050     │  PB6/SCL  PB7/SDA  │  Cortex-M3 72MHz │
         └──────────────┘                    │                  │
                                             │  TIM2 1kHz 中断  │
                                             │   ↓ 采样入环形窗  │
                                             │  libneai.a 推理  │
                                             └────────┬─────────┘
                                                      │ USART1 115200 8N1
                                                      ▼
                                          PC 串口助手（CSV / 相似度 / 告警）
```

三步走的开发路线，每一步都有对应的详细文档：

| 阶段 | 目标 | 文档 |
| --- | --- | --- |
| 第一步 | 串口读出三轴加速度，验证传感器 + I2C + 串口链路 | [README-第一步](project1/README-第一步-串口读取MPU6050.md) |
| 第二步 | TIM2 定时 1 kHz 采样 → 命令触发录制 1024×3 窗口 → 导出 CSV 打标 | [README-第二步](project1/README-第二步-定时采样与CSV数据采集.md) |
| 第三步 | NanoEdge 训练 → 生成 AI 库 → 板上学习 + 实时检测 + 告警 | [README-第三步](project1/README-第三步-NanoEdge训练与板上部署.md) |

### 串口命令（第三步固件）

| 命令 | 作用 |
| --- | --- |
| `s` | 实时流，100 Hz 打印三轴原始值 |
| `r` | 录制一个窗口（1024 样本 × 3 轴 ≈ 1.3 s 输出） |
| `l` | 用最近录制的窗口学习正常态（自动切成模型要求长度的子窗口） |
| `d` | 开/关连续检测模式：录窗口 → 推理 → 打印相似度 → 超阈值告警 |
| `c` | 取消当前录制 |
| `h` | 帮助 |

---

## 3. 硬件清单

| 器件 | 说明 | 参考价 |
| --- | --- | --- |
| STM32F103C8T6 Blue Pill | 主控，Cortex-M3 @72 MHz，64 KB Flash / 20 KB RAM | ~10 元 |
| GY-521（MPU6050） | 三轴加速度计，I2C 地址 0x68，±2 g 量程 | ~5 元 |
| ST-Link V2 | SWD 烧录/调试 | ~15 元 |
| USB-TTL（CH340/CP2102） | 读串口数据（Blue Pill 板载 USB 不是串口桥） | ~5 元 |
| 小直流电机 / USB 风扇 | 振动源，5~15 元；用双面胶把传感器贴在外壳上 | ~10 元 |

接线见 [第一步文档](project1/README-第一步-串口读取MPU6050.md#1-接线)。故障工况用「叶片贴配重（胶带/橡皮泥）或卡异物改变负载」制造。

---

## 4. 目录结构

```
.
├── README.md                                    # 本文件：项目总览
├── .gitignore                                   # 排除编译产物（源码/数据/AI 库保留）
│
├── led/                                         # 板级点灯验证（CubeMX + Keil MDK 工程）
│   └── stm32cubemx_test/stm32projects/test_LED/
│       ├── test_LED.ioc                         # CubeMX 配置
│       ├── Core/{Inc,Src}/                      # GPIO + HAL 初始化、main.c
│       ├── Drivers/                             # STM32F1xx HAL + CMSIS
│       └── MDK-ARM/test_LED.uvprojx             # Keil 工程文件（编译产物已忽略）
│
└── project1/                                    # 主项目：振动异常检测
    ├── README-第一步-串口读取MPU6050.md
    ├── README-第二步-定时采样与CSV数据采集.md
    ├── README-第三步-NanoEdge训练与板上部署.md
    │
    ├── stm32cubeide_test/mpu6050_test/mpu6050_ai_test/   # 板上固件（STM32CubeIDE / GCC）
    │   ├── Core/Src/main.c                      # 初始化 + 主循环 + 中断回调
    │   ├── Core/Src/collector.c                 # 采样/录制/学习/检测状态机（含 ALARM_THRESHOLD）
    │   ├── Core/Src/mpu6050.c                   # 传感器驱动
    │   ├── Core/Inc/NanoEdgeAI.h                # NanoEdge 库接口（含信号长度宏）
    │   ├── NEAI/libneai.a                       # 训练生成的 AI 静态库
    │   └── mpu6050_ai_test.ioc                  # CubeMX 配置（I2C1/TIM2/USART1/PC13）
    │
    └── NanoEdge ai_math/project_1/              # 数据集 + 训练产物
        ├── normal_01..05.csv                    # 正常工况（各 1024 行 × 3 列）
        ├── fault_01..05.csv                     # 故障工况（各 1024 行 × 3 列）
        ├── libneai_nanoedge-ai-test-try1_2.zip  # Studio 导出的原始包
        └── libneai_nanoedge-ai-test-try1_2/     # 解包内容
            ├── libneai.a / NanoEdgeAI.h
            ├── metadata.json                    # 模型参数、内存占用、得分、user_info
            ├── artifacts/                       # 预处理/MLL 参数快照
            └── emulators/                       # 上位机模拟器（可选，用 Python 验证模型）
```

---

## 5. 快速开始

### 5.1 编译烧录主固件（第三步）

1. 安装 **STM32CubeIDE**，`File → Open Projects from File System` 选中
   `project1/stm32cubeide_test/mpu6050_test/mpu6050_ai_test`。
2. 链接 AI 库（CubeIDE 的编译在 `Debug/` 里执行，路径要从工程根算）：
   `Project → Properties → C/C++ Build → Settings → Tool Settings → MCU/MPU GCC Linker → Libraries`
   清空 **Libraries (-l)** 与 **Library search path (-L)**，改在
   **Miscellaneous → Other flags** 里填 `-L../NEAI -lneai`。
3. `Build` → `Run`，串口（115200 8N1）应打印：

```
=== MPU6050 AI Detector v3 ===
Cmd: s=stream r=record l=learn d=detect c=cancel h=help
[AI] NanoEdge init OK, signal len=1
```

4. 学习正常态：电机稳定运转 → 发 `r` → 发 `l`，重复 3~5 次。
5. 开检测：发 `d`；正常时相似度接近 100，给电机加故障后相似度下降并触发告警。

> ⚠️ 模型知识存在 **RAM**，掉电即失，重启后需重新执行学习流程（进阶可用
> `neai_anomalydetection_get_knowledge/set_knowledge` 存进内部 Flash 自动恢复）。

### 5.2 编译点灯验证（led/）

用 **Keil MDK** 打开 `led/stm32cubemx_test/stm32projects/test_LED/MDK-ARM/test_LED.uvprojx` 直接编译下载；或改 `.ioc` 后用 CubeMX 重新生成。

### 5.3 只验证模型（不接板子）

```bash
cd "project1/NanoEdge ai_math/project_1/libneai_nanoedge-ai-test-try1_2/emulators"
python nanoedgeai_studio_emulator.py    # 按脚本内说明加载 artifacts 与 CSV 验证
```

---

## 6. 数据集与模型

- **数据集**：`normal_01~05.csv`（健康）、`fault_01~05.csv`（故障），每个文件 = 一个信号窗口
  （1024 行 × 3 列，无表头，单位是 MPU6050 原始 LSB，±2 g 下 1 g ≈ 16384）。
- **训练配置**：NanoEdge AI Studio 5.2.0，Anomaly Detection / on-board training，
  target = STM32F1 Series（Cortex-M3），max RAM 20000 B、max Flash 64000 B。
- **选中算法**：MML（预处理 = 绝对值 + 32% 移动平均池化），`signal length = 1 × 3 轴`，
  `learning_min_lines = 56`。
- **得分**：main KPI **0.9934**，综合 score 0.9806；预计占用 RAM 624 B / Flash 564 B。

详细参数见 [metadata.json](project1/NanoEdge%20ai_math/project_1/libneai_nanoedge-ai-test-try1_2/metadata.json)。

---

## 7. 关键参数速查

| 参数 | 值 | 位置 |
| --- | --- | --- |
| 采样率 | 1 kHz（TIM2：PSC=71，ARR=999） | `mpu6050_ai_test.ioc` / `tim.c` |
| 窗口长度 | `COLLECT_SAMPLES 1024` × `COLLECT_AXES 3`（6 KB RAM 缓冲） | `Core/Inc/collector.h` |
| 告警阈值 | `ALARM_THRESHOLD 80` | `Core/Src/collector.c` |
| 串口 | USART1，115200 8N1，PA9/PA10 | `usart.c` |
| 传感器 | MPU6050 @ I2C1，PB6/PB7，400 kHz，Fast Mode | `i2c.c`、`mpu6050.c` |
| 告警指示 | PC13 板载 LED 闪 3 次 | `collector.c` |

---

## 8. 已知限制与后续方向

- 知识存 RAM，掉电丢失 → 可接 `get/set_knowledge` 落 Flash 做免学习重启。
- 数据集每类仅 5 个窗口（Studio 侧已自动切分训练/验证），样本量偏小；
  扩到每类 15~20 个窗口可进一步稳住区分度。
- 阈值依赖现场工况标定，建议实测正常/故障相似度分布后再定（默认 80）。
- 可加：电机调速（MOSFET 或 L298N/TB6612）做多转速工况、SD 卡本地留证、
  OLED 本地显示、报警联动。

---

## 9. 参考资料

- ST Wiki：*NanoEdge AI Library for anomaly detection*（API、链接方式、阈值建议）
- ST Wiki：*How to Build an Anomaly Detection Project for Predictive Maintenance with NanoEdge AI Studio*
- STM32F103C8T6 数据手册 / RM0008 参考手册；MPU6050 数据手册

---

*本仓库为课程/实践项目归档：硬件验证、数据采集、边缘 AI 训练与部署的完整闭环。*
