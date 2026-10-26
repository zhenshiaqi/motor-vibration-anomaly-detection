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
| 告警 | 相似度低于阈值（默认 80）→ 串口 `[ALARM]` + 板载 LED（PC13）闪 3 次（非阻塞，不打断采样） |
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
├── firmware/                                    # 预编译固件：不装 IDE 也能直接烧板
│   ├── README.md                                # 烧录步骤 + 开机自检 + LED 状态码
│   ├── mpu6050_ai_test_v4.1.hex/.bin/.elf       # v4.1（推荐）：开机自检 + 非阻塞告警灯
│   └── legacy/                                  # v3 历史固件（仅供对照，不建议烧）
│       ├── README.md
│       └── mpu6050_ai_test_v3.hex/.bin/.elf
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
2. 检查 AI 库链接（工程里**已经配好**，仅在重新生成工程或换库时才需要改）：
   `Project → Properties → C/C++ Build → Settings → Tool Settings → MCU/MPU GCC Linker →
   Libraries` 里 **Libraries (-l)** 与 **Library search path (-L)** 保持为空，
   改在 **Miscellaneous → Other flags** 里填 `-L../NEAI -lneai`
   （CubeIDE 的编译在 `Debug/` 里执行，所以相对路径要从 `Debug` 往上退一级）。
3. `Build` → `Run`。**先把串口助手打开（115200 8N1）再复位板子**，否则会错过开机日志：

```
=== MPU6050 AI Detector v4.1 ===
UART OK: USART1 PA9(TX)/PA10(RX) 115200 8N1
[CLK] HSE 8MHz -> PLL 72MHz
[BOOT] LED selftest done (PC13), init MPU6050...
[I2C] MPU6050 OK, WHO_AM_I=0x68
Cmd: s=stream r=record l=learn d=detect c=cancel h=help
[I2C] MPU6050 online, sampling 1kHz (TIM2)
[AI] NanoEdge init OK, signal len=1
[ready] type h for help
```

4. 学习正常态：电机稳定运转 → 发 `r` → 发 `l`，重复 3~5 次。
5. 开检测：发 `d`；正常时相似度接近 100，给电机加故障后相似度下降并触发告警。

> ⚠️ 模型知识存在 **RAM**，掉电即失，重启后需重新执行学习流程（进阶可用
> `neai_anomalydetection_get_knowledge/set_knowledge` 存进内部 Flash 自动恢复）。

### 5.2 不装 IDE，直接烧预编译固件（firmware/）

用 **STM32CubeProgrammer** 打开 `firmware/mpu6050_ai_test_v4.1.hex` → Download →
断电重上电，串口 115200 8N1 就能看到上面的开机自检日志。不会编译的人走这条路最快。

> 早期 `led/` 点灯验证工程（CubeMX + Keil MDK）**没有上传到仓库**，只在作者本地保留，
> 并已写进 `.gitignore`；板载 LED 的点灯自检现在由主固件开机时完成（闪 3 次），
> 不再需要单独的 led 工程。

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
| 传感器 | MPU6050 @ I2C1，PB6/PB7，400 kHz，Fast Mode，地址 0x68（AD0 接 GND） | `i2c.c`、`mpu6050.c` |
| 传感器在线检测 | 读 `WHO_AM_I(0x75)`：0x68 = MPU6050，0x70~0x74 = MPU6500/9250 兼容 | `mpu6050.c` |
| I2C 超时 | `MPU6050_I2C_TIMEOUT_MS 50`（不是 HAL_MAX_DELAY，避免总线卡死后"假死"） | `Core/Inc/mpu6050.h` |
| 告警指示 | PC13 板载 LED 非阻塞闪 3 次（100 ms/拍）+ 串口 `[ALARM]` | `collector.c` |
| 空闲心跳 | 每 5 s 打印 `[alive] idle, type h for help` | `collector.h`（`HEARTBEAT_MS`） |

---

## 8. 现场排查：串口没输出 / 传感器没找到

**先记住一件事：v4.1 固件只在"上电那一下"打印开机日志，之后空闲时每 5 秒才有一行心跳。**
所以正确的测试姿势是：**先打开串口助手（115200 8N1，选对 COM 口），再按板子上的复位键**，
不要反过来。

### 8.1 看 LED（PC13）就能定位到哪一步

PC13 是 Blue Pill 板载 LED，**低电平点亮**。

| LED 表现 | 说明 | 下一步 |
| --- | --- | --- |
| 上电快闪 3 次 → 熄灭 | 正常，MCU/时钟/串口/传感器都在线 | 发 `h` 应回帮助；发 `r`→`l`→`d` 走流程 |
| 上电快闪 3 次 → **常亮** | MCU 正常，**MPU6050 没找到** | 查 VCC=3.3V、GND、SCL=PB6、SDA=PB7、AD0 接 GND |
| 约 5 Hz **持续快闪**，串口无字 | 外设初始化失败（`Error_Handler`），或 HSE+HSI 都起不来 | 多半是板子/供电问题；用 ST-Link 重新烧一次 |
| **完全不亮**，串口无字 | 没供电 / 没烧进去 / BOOT 跳线错 / 串口线错 | 先量 3.3V，再用 CubeProgrammer 读回芯片确认有程序 |

### 8.2 串口本身的三个高频错误

1. **TX/RX 没交叉**：USB-TTL 的 **RXD 接 PA9（TX）**，**TXD 接 PA10（RX）**；
2. **没共地**：USB-TTL 的 GND 必须和 Blue Pill 的 GND 连在一起；
3. **把板载 USB 当串口**：Blue Pill 的 USB 口不是 USB-TTL，插上也不会出现 COM 口，
   必须用独立的 CH340/CP2102 模块（或带 VCP 的 ST-Link）。

### 8.3 已经烧了 v3，串口一行都没有？

v3 的打印全部在 `Collector_Init()` 里，而它只在 MPU6050 初始化成功后才被调用：

- 传感器没接好 → 只打印一行 `MPU6050 init FAIL`，之后就"装死"（命令也被忽略）；
- 如果串口助手是在这行打印**之后**才打开的，就完全看不到任何输出；
- HSE 晶振起不来 → 直接进 `Error_Handler()` 死循环，同样一个字都没有。

换成 `firmware/mpu6050_ai_test_v4.1.hex` 即可区分这三种情况（见 8.1）。

> 完整的排查过程、代码级分析与验证证据，见 [docs/audit-report.md](docs/audit-report.md)。

---

## 9. 已知限制与后续方向

- 知识存 RAM，掉电丢失 → 可接 `get/set_knowledge` 落 Flash 做免学习重启。
- 数据集每类仅 5 个窗口（Studio 侧已自动切分训练/验证），样本量偏小；
  扩到每类 15~20 个窗口可进一步稳住区分度。
- 阈值依赖现场工况标定，建议实测正常/故障相似度分布后再定（默认 80）。
- 可加：电机调速（MOSFET 或 L298N/TB6612）做多转速工况、SD 卡本地留证、
  OLED 本地显示、报警联动。

---

## 10. 参考资料

- ST Wiki：*NanoEdge AI Library for anomaly detection*（API、链接方式、阈值建议）
- ST Wiki：*How to Build an Anomaly Detection Project for Predictive Maintenance with NanoEdge AI Studio*
- STM32F103C8T6 数据手册 / RM0008 参考手册；MPU6050 数据手册

---

## 11. 版本变更

**v4.1（当前源码 + `firmware/mpu6050_ai_test_v4.1.*`）**

- 修正：HSE 晶振起不来回退到 HSI 64 MHz 时，TIM2 预分频按实际 PCLK1 重算
  （64 MHz → PSC=63），采样率仍是 1 kHz；v4 在同样情况下会掉到约 889 Hz。
- 目录整理：`firmware/` 只保留当前版本，v3 归档到 `firmware/legacy/`。

**v4（已被 v4.1 取代，二进制可从 git 历史取回）**

- 开机日志/命令表不再依赖传感器初始化：先打印横幅、闪灯自检，再初始化 I2C；
  传感器失败也照常进入命令循环（v3 会整个 CLI 失效，表现为"串口没输出"）。
- HSE 晶振起不来时自动回退到 HSI 64 MHz，不再静默进 `Error_Handler`。
- I2C 全部改有限超时（50 ms），新增 `WHO_AM_I` 校验和 `MPU6050_BusRecover()` 总线自救。
- 告警灯改非阻塞 100 ms 状态机（原来 `HAL_Delay` 600 ms 会卡住采样/串口）。
- 空闲每 5 秒打印 `[alive]` 心跳；`Error_Handler` 用 5 Hz 闪灯代替死循环。
- 文档：删除已下线 `led/` 工程的引用，补 `firmware/` 目录、开机自检与 LED 状态码。

**v3（`firmware/legacy/mpu6050_ai_test_v3.*`）**

- 首次完整发布版本：1 kHz 采样、板上学习、实时检测、`[ALARM]` + LED 闪 3 次。

---

## 12. 许可证

本项目采用**三选一（multiple / disjunctive license）**：你可以任选下面之一来使用本仓库的
原创代码与数据，选哪个就以哪个的条款为准。

| 许可证 | 文件 | 适合场景 |
| --- | --- | --- |
| MIT | [`LICENSE-MIT`](LICENSE-MIT) | 最宽松，署名即可；课程作业、作品集、二次开发都省事 |
| Apache-2.0 | [`LICENSE-APACHE`](LICENSE-APACHE) | 比 MIT 多一条明确的专利授权，适合想保留专利保护的场景 |
| GPL-3.0（only） | [`LICENSE-GPL-3.0`](LICENSE-GPL-3.0) | 传染性开源：衍生作品也必须开源 |

SPDX 标识：`MIT OR Apache-2.0 OR GPL-3.0-only`
版权声明：`Copyright (c) 2026 zhenshiaqi`

> ⚠️ 上面三选一只覆盖**本项目原创部分**（`Core/Src`、`Core/Inc`（除 `NanoEdgeAI.h`）、
> 文档、CSV 数据集、由这些源码编译出的 `firmware/` 固件）。
> 第三方组件仍按各自许可证：ST 的 STM32F1 HAL/CMSIS 是 BSD-3-Clause
> （见 `Drivers/*/LICENSE.txt`），`NEAI/libneai.a` 与 `NanoEdgeAI.h` 归 STMicroelectronics。
> 完整说明见 [`LICENSE`](LICENSE)。

---

*本仓库为课程/实践项目归档：硬件验证、数据采集、边缘 AI 训练与部署的完整闭环。*
