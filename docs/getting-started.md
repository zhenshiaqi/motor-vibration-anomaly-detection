# 获取与使用指南（Getting Started）

> 面向第一次接触本项目、想"下载完就把板子跑起来"的人。
> 对应固件：**v4.1**（`firmware/mpu6050_ai_test_v4.1.hex`）
> 更深入的问题排查见 [`audit-report.md`](audit-report.md)，工程细节见仓库根目录 [`README.md`](../README.md)。

---

## 0. 30 秒回答：下载之后能直接干什么

| 你想做的事 | 能不能直接成 | 说明 |
| --- | --- | --- |
| 下载 → 烧 `firmware/mpu6050_ai_test_v4.1.hex` → 串口看到开机日志 | ✅ 可以 | 唯一容易踩的坑：**先打开串口助手（115200 8N1）再给板子上电** |
| 下载源码 → 用 STM32CubeIDE 自己编译 | ✅ 可以 | 链接参数已在 `.cproject` 里配好；任意路径克隆都能编过 |
| 下载 → 接上电机 → 自动检测/报警 | ❌ 不行 | 必须自己走一遍 `r`（录 1 秒）→ `l`（学习 3~5 次）→ `d`（开检测）；并且**每次上电都要重学** |

一句话：**硬件接对 + 按顺序操作，就能跑；它是"需要现场学习一次"的异常检测，不是开箱即判的固定阈值报警器。**

---

## 1. 准备清单

### 1.1 硬件（4 件，合计约 35 元）

| 器件 | 型号/规格 | 作用 | 参考价 | 注意 |
| --- | --- | --- | --- | --- |
| 主控板 | STM32F103C8T6 "Blue Pill" | 采样 + 跑 AI 模型 | ~10 元 | 板载 USB 口**不是**串口，别指望插上就出 COM 口 |
| 传感器 | GY-521（MPU6050）三轴加速度计 | 测振动 | ~5 元 | I2C 地址 0x68（AD0 接 GND） |
| 下载器 | ST-Link V2（SWD 四线） | 烧录/调试 | ~15 元 | 带 VCP 虚拟串口的型号可以省掉 USB-TTL |
| 串口模块 | USB-TTL（CH340 / CP2102） | 看数据、发命令 | ~5 元 | 必须与主板**共地** |
| 振动源 | 小直流电机 / USB 风扇 | 被监测对象 | ~10 元 | 传感器用双面胶**贴紧**外壳 |
| 杜邦线 | 母对母若干 | 接线 | — | 接触不良是"读不到传感器"的常见原因 |

### 1.2 软件（2 个）

| 软件 | 用途 | 说明 |
| --- | --- | --- |
| **STM32CubeProgrammer** | 烧 `.hex` / `.bin` | 推荐；也可用 ST-Link Utility 或 Keil |
| **串口助手** | 115200 8N1 收发 | XCOM / SSCOM / VS Code 的 Serial Monitor 都行 |
| STM32CubeIDE（可选） | 自己编译、改代码 | 只有要改源码时才需要 |

### 1.3 **不需要**的东西

- 不需要 OLED、SD 卡、电机驱动板、蜂鸣器；
- 不需要联网、不需要云服务，推理全在板子上；
- **只使用发布固件的话，不需要 ST 账号、不需要装 NanoEdge AI Studio**（那是重新训练模型时才用的）。

---

## 2. 接线

### 2.1 GY-521 ↔ Blue Pill（I2C1）

| GY-521 | Blue Pill |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SCL | **PB6**（I2C1_SCL） |
| SDA | **PB7**（I2C1_SDA） |
| AD0 | GND（决定地址 0x68；接 VCC 会变 0x69，固件认不到） |

### 2.2 ST-Link ↔ Blue Pill（SWD，4 根）

| ST-Link | Blue Pill |
| --- | --- |
| 3.3V | 3.3V |
| GND | GND |
| SWDIO | DIO（PA13） |
| SWCLK | CLK（PA14） |

### 2.3 USB-TTL ↔ Blue Pill（读串口用）

| USB-TTL | Blue Pill |
| --- | --- |
| RXD | **PA9**（USART1_TX） |
| TXD | **PA10**（USART1_RX） |
| GND | GND（**必须共地**） |

> ⚠️ 三个最高频的接线错误：
> 1. **TX/RX 没交叉**：模块 RXD 要接板子的 TX（PA9），不是接 PA9 的"TX 脚对 TX 脚"；
> 2. **没共地**：只接 RXD/TXD 不接 GND，串口助手只会收到乱码或什么都没有；
> 3. **把板载 USB 当串口**：Blue Pill 的 USB 口不是 USB-TTL，插上不会出现 COM 口。

### 2.4 机械安装

用双面胶/扎带把 GY-521 **贴紧**电机或风扇外壳（不要悬空、不要只粘一根线）。传感器方向决定哪个轴感受主振动，平放时重力落在 Z 轴（约 16384 LSB）。

---

## 3. 烧录（两条路，任选一条）

### 3.1 用发布好的固件（推荐，不用装 IDE）

1. 打开 [Releases](https://github.com/zhenshiaqi/motor-vibration-anomaly-detection/releases) 页面，下载 `mpu6050_ai_test_v4.1.hex`；
   （也可以直接用仓库里的 `firmware/mpu6050_ai_test_v4.1.hex`，内容相同）
2. ST-Link 接好 SWD → 打开 STM32CubeProgrammer → 选 **ST-LINK** → **Connect**；
3. 打开 hex → **Download**；
4. 断电重上电（或在 CubeProgrammer 里复位）。

### 3.2 自己编译

1. STM32CubeIDE：`File → Open Projects from File System`，选中
   `project1/stm32cubeide_test/mpu6050_test/mpu6050_ai_test`；
2. 确认 `Project → Properties → C/C++ Build → Settings → Tool Settings → MCU/MPU GCC Linker →
   Miscellaneous → Other flags` 里有 `-L../NEAI -lneai`（仓库里已配好，一般不用动）；
3. `Build` → `Run`；
4. 产物在 `Debug/`，可以复制到 `firmware/` 留档。

> 注意：v4.1 改了 `Collector_Init()` 的签名（多了 `sensor_ok`），
> `main.c` / `collector.c` / `collector.h` 必须一起替换，只换一个会编译不过。

---

## 4. 上电：应该看到什么

**先把串口助手打开（115200、8N1、选对 COM 口），再给板子上电。** 固件只在启动时打印一次菜单。

正常日志：

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

之后空闲时每 5 秒会有一行 `[alive] idle, type h for help`，表示固件还在跑。

### LED（PC13）状态码：不看串口也能判断

PC13 是 Blue Pill 板载 LED，**低电平点亮**。

| LED 表现 | 含义 | 下一步 |
| --- | --- | --- |
| 快闪 3 次 → 熄灭 | 正常（MCU、时钟、串口、传感器都在线） | 发 `h`，开始第 5 节流程 |
| 快闪 3 次 → **常亮** | MCU 正常，但 **MPU6050 没找到** | 查 VCC=3.3V、GND、SCL/SDA、AD0 |
| 约 5 Hz 持续快闪，串口无输出 | 外设/时钟初始化失败（`Error_Handler`） | 重新烧录；检查供电 |
| 完全不亮 | 没供电 / 没烧进去 / BOOT 跳线错 | 量 3.3V，用 CubeProgrammer 确认芯片里有程序 |
| 检测中闪 3 次（100 ms 一拍） | 告警：相似度低于阈值 | 见第 5 节第 5 步 |

---

## 5. 第一次跑通：5 分钟验收

1. **确认链路**：串口助手发 `h` + 回车，应回 `[Help] s:stream r:record l:learn d:detect c:cancel h:help`。
2. **看原始数据**：发 `s` 打开实时流，应每秒约 100 行 `AX:… AY:… AZ:…`；平放时 `AZ` 约 16000；用手轻敲桌子，数值明显跳变。再发 `s` 关闭。
3. **录一个正常窗口**：让电机**正常运转**并稳定 → 发 `r`（约 1 秒）→ 窗口录满后自动回一段 `====BEGIN==== … ====END====` 的 CSV（如果不在检测模式）。**注意：`r` 之后看到 CSV 是正常的，用于导出数据集；学习用的是板内缓冲。**
4. **学习正常态**：发 `l`，应回 `[AI] learned N signal(s)`。**重复「`r` → `l`」3~5 次**，让模型见够正常工况。
5. **开检测**：发 `d`。正常时打印 `SIM:80+`（越接近 100 越正常）。
   制造故障（叶片贴配重 / 卡异物 / 改变负载），相似度应下降；低于 80 时打印
   `[ALARM] anomaly detected! (PC13 LED blinks 3x)` 且 LED 闪 3 次。
6. 再次发 `d` 关闭检测。

> ⚠️ **每次断电/复位后，模型知识都会丢**（存在 RAM）。重新上电后要重新 `r → l` 几轮再 `d`。

---

## 6. 命令速查

| 命令 | 作用 | 典型输出 |
| --- | --- | --- |
| `s` | 开/关实时流（约 100 Hz） | `AX:-123 AY:456 AZ:16321` |
| `r` | 录一个 1024 样本 × 3 轴窗口（@1 kHz ≈ 1 秒） | `[Rec] start recording 1024 samples...` |
| `l` | 用最近录制的窗口学习正常态 | `[AI] learned 1024 signal(s)` |
| `d` | 开/关连续检测（自动录音 → 推理 → 打印相似度） | `SIM:97` / `[ALARM] anomaly detected! ...` |
| `c` | 取消当前录制 | `[Rec] cancelled` |
| `h` | 帮助 | `[Help] ...` |

> `l` 的 `N` 取决于模型要求的输入长度（本仓库模型是 `signal len = 1 × 3 轴`，
> 一个 1024 样本窗口会被切成 1024 个信号逐个学习）。

---

## 7. 常见问题

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 串口一个字都没有 | 串口助手在打印之后才打开 / TX-RX 没交叉 / 没共地 / 波特率错 | **先开助手再上电**；模块 RXD→PA9、TXD→PA10、GND 共地；115200 8N1 |
| 日志停在 `[I2C] MPU6050 no answer (1/3)…` | 传感器没供电 / SCL-SDA 接反 / AD0 接高（0x69） | 量模块 VCC=3.3V；SCL→PB6、SDA→PB7、AD0→GND |
| `[I2C] MPU6050 OK` 但 `[AI] NanoEdge init FAIL` | AI 库没链进去 | 检查 Other flags 里的 `-L../NEAI -lneai`，且 `NEAI/libneai.a` 存在 |
| 开机显示 `[CLK] HSE/8MHz crystal failed -> fallback to HSI 64MHz` | 板子晶振没起振（克隆板/虚焊） | 仍能正常用（v4.1 已保证 1 kHz）；条件允许换块晶振正常的板 |
| `SIM` 一直是 0 或很低，正常态也报警 | 还没学习 / 学习时电机不是正常工况 / 阈值不合适 | 先 `r`→`l` 学 3~5 次；确认学习时电机正常运转；必要时调 `ALARM_THRESHOLD` |
| 正常/故障的 SIM 区分不开 | 传感器没贴紧、工况差异小、数据样本少 | 传感器贴牢；故障工况更明显（加配重）；多录几组再学 |
| 烧录后再也连不上 | SWD 引脚被禁用 / 程序进入低功耗 | BOOT0 拉高上电后重烧，再把 SYS→Debug 设为 Serial Wire |
| 上电后又要重新学 | 正常现象：知识存 RAM，掉电即失 | 每次上电执行 `r → l`；想免学习需把 knowledge 写进 Flash（见第 9 节） |

---

## 8. 能力边界（别期待它做什么）

- **不是固定阈值报警器**：它先"学习"你这台设备的正常振动，再判断偏离程度；换设备/换转速要重新学。
- **知识不持久**：存在 RAM，掉电丢失。
- **只用加速度计**：陀螺仪数据没参与模型。
- **1 kHz / 1024 样本窗口**：适合小型电机、风扇这类中高频振动；超低频或冲击类故障未必合适。
- **数据集很小**（正常/故障各 5 个窗口）：足以演示闭环，正式使用建议每类扩到 15~20 个窗口重新 Benchmark。
- **阈值 80 是起点**：不同电机的最佳阈值不同，按实测 SIM 分布调。

---

## 9. 想改点什么

| 想做的事 | 改哪里 | 说明 |
| --- | --- | --- |
| 调告警灵敏度 | `Core/Src/collector.c` 的 `ALARM_THRESHOLD`（默认 80） | 正常 SIM 常低于 80 → 调低；故障 SIM 仍高于阈值 → 调高 |
| 改窗口/采样 | `Core/Inc/collector.h` 的 `COLLECT_SAMPLES`；TIM2 的 PSC/ARR | 改窗口要同步改模型输入或重新训练 |
| 关掉空闲心跳 | `Core/Inc/collector.h` 的 `HEARTBEAT_MS` 设为 0 | 心跳每 5 秒一行 |
| 重新训练模型 | 见 [第三步文档](../project1/README-第三步-NanoEdge训练与板上部署.md) | 需要 ST 账号 + NanoEdge AI Studio |
| 免去每次上电重学 | 用 `neai_anomalydetection_get_knowledge/set_knowledge` 存内部 Flash | 进阶改造，仓库尚未实现 |
| 多转速/多工况 | 加电机调速（MOSFET / TB6612） | 见根 README「后续方向」 |

---

## 10. 相关文档

| 文档 | 内容 |
| --- | --- |
| [`README.md`](../README.md) | 项目总览、架构、目录结构、版本变更 |
| [`firmware/README.md`](../firmware/README.md) | 固件清单、烧录、上电日志、LED 状态码 |
| [`project1/README-第一步-串口读取MPU6050.md`](../project1/README-第一步-串口读取MPU6050.md) | 从零搭工程、读三轴数据 |
| [`project1/README-第二步-定时采样与CSV数据采集.md`](../project1/README-第二步-定时采样与CSV数据采集.md) | 1 kHz 定时采样、导出 CSV |
| [`project1/README-第三步-NanoEdge训练与板上部署.md`](../project1/README-第三步-NanoEdge训练与板上部署.md) | NanoEdge 训练、集成、板上学习与检测 |
| [`audit-report.md`](audit-report.md) | "串口没输出"的代码级排查与验证证据 |
| [`../LICENSE`](../LICENSE) | 许可证（MIT / Apache-2.0 / GPL-3.0 三选一） |

---

*文档版本：2026-10-27（对应固件 v4.1）。若固件更新，以 `firmware/README.md` 的上电日志为准。*
