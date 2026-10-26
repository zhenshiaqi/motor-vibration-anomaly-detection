# 固件（可直接烧录）

本目录是 `project1/stm32cubeide_test/mpu6050_test/mpu6050_ai_test/` 编译出的产物，
方便不装 IDE 也能直接烧板验证。同名三种格式内容等价，按工具选用其一。

| 文件 | 大小 | 用途 |
| --- | --- | --- |
| `mpu6050_ai_test_v4.elf` | 923 KB | **推荐**：带符号与调试信息，CubeIDE / GDB 调试用（含源码行号） |
| `mpu6050_ai_test_v4.hex` | 89 KB | **推荐**：Intel HEX，STM32CubeProgrammer、ST-Link Utility、Keil 都能直接下载 |
| `mpu6050_ai_test_v4.bin` | 31 KB | **推荐**：裸二进制，只能烧到 `0x08000000` 起始地址 |
| `mpu6050_ai_test.elf/.hex/.bin` | 900/81/29 KB | v3 历史版本，保留用于对照（无开机自检，见下） |

> **v3 → v4 为什么值得换**：v3 把横幅和命令表都放在 `Collector_Init()` 里，
> 而 `Collector_Init()` 只有 MPU6050 初始化成功才会被调用。传感器一没接好，
> 板子就一行字都不打印，现场根本分不清"固件没跑 / 串口接错 / 传感器坏了"。
> v4 先打印、先闪灯证明 MCU 和串口活着，再去碰 I2C；传感器失败也照常进命令循环，
> 并且会明确打印失败原因。

## 构建信息（v4）

| 项 | 值 |
| --- | --- |
| 目标芯片 | STM32F103C8Tx（Cortex-M3，64 KB Flash / 20 KB RAM） |
| 构建工具 | STM32CubeIDE 2.2.0 + GNU Tools for STM32 14.3.rel1（arm-none-eabi-gcc） |
| 架构 | `elf32-littlearm`，armv7-m，`-mcpu=cortex-m3 -mthumb` |
| Flash 占用 | **32,296 B / 65,536 B ≈ 49%**（text 32,200 + data 92） |
| RAM 占用 | **9,088 B / 20,480 B ≈ 44%**（data 92 + bss 8,996，另有 1 KB 栈 + 0.5 KB 堆） |
| 对应源码 | 仓库 `main` 分支 v4 源码（`main.c` / `collector.c` / `mpu6050.c`） |

其中 RAM 的大头是采集缓冲 `int16_t buf[1024][3]`（6 KB）加 NanoEdge 库运行时开销。

> v3（`mpu6050_ai_test.*`）：Flash 29,620 B / RAM 9,080 B，功能相同但没有开机自检。

## 烧录

**STM32CubeProgrammer**（推荐，支持 hex/bin/elf 全格式）：

1. ST-Link 连板子（SWDIO/PA13、SWCLK/PA14、3.3V、GND）；
2. 打开 CubeProgrammer → 选 ST-LINK → Connect；
3. 打开 `mpu6050_ai_test_v4.hex` → **Download**；
4. 断电重上电。**先把串口助手打开（115200 8N1）再上电**，否则会错过开机那几行日志。

**STM32CubeIDE**：`Run → Run Configurations` 里把固件指向 `.elf` 也能下载。

## 上电应该看到什么

```
=== MPU6050 AI Detector v4 ===
UART OK: USART1 PA9(TX)/PA10(RX) 115200 8N1
[CLK] HSE 8MHz -> PLL 72MHz
[BOOT] LED selftest done (PC13), init MPU6050...
[I2C] MPU6050 OK, WHO_AM_I=0x68
Cmd: s=stream r=record l=learn d=detect c=cancel h=help
[I2C] MPU6050 online, sampling 1kHz (TIM2)
[AI] NanoEdge init OK, signal len=1
[ready] type h for help
```

之后板子空闲时每 5 秒还会打印一行 `[alive] idle, type h for help`，
用来证明固件仍在跑、串口链路没断。

传感器没接好时，会看到：

```
[I2C] MPU6050 no answer (1/3). check VCC/GND/SCL=PB6/SDA=PB7/AD0
[I2C] MPU6050 no answer (2/3). ...
[I2C] MPU6050 no answer (3/3). ...
[I2C] MPU6050 NOT FOUND. CLI stays alive; s/r/d only report the error.
```

此时命令 `h` / `l` 仍然有回应，`s`/`r`/`d` 会直接告诉你传感器不在线 —— 不会再"装死"。

## LED（PC13）状态码

PC13 是 Blue Pill 板载 LED，**低电平点亮**。不用串口，看灯也能判断卡在哪一步：

| LED 表现 | 含义 |
| --- | --- |
| 上电后快闪 3 次，然后熄灭 | 正常：MCU + 时钟 + 串口初始化完成，传感器也在线 |
| 上电后快闪 3 次，然后**常亮** | MCU 正常，但 **MPU6050 没找到**（接线/供电/地址问题） |
| 约 5 Hz 持续快闪，串口无任何输出 | 外设或时钟初始化失败（`Error_Handler`），或 HSE 与 HSI 都起不来 |
| 完全不亮、串口无输出 | 没供电 / 没烧进去 / BOOT 跳线不对 / 串口线接错 |
| 检测中告警时闪 3 次（100 ms 一拍） | 相似度低于阈值，检出了异常 |

## 上板后怎么用

```
r 录 1 秒窗口 → l 学习（正常工况下重复 3~5 次）
d 开关连续检测：正常 SIM:80+，加故障后相似度下降并打印 [ALARM]，PC13 LED 闪 3 次
```

> ⚠️ 模型知识存在 **RAM**，掉电即失。每次重新上电都要重新执行 `r → l` 学习流程。
> 详细说明见 [第三步文档](../project1/README-第三步-NanoEdge训练与板上部署.md)。

## 重新构建

用 CubeIDE 打开 `project1/stm32cubeide_test/mpu6050_test/mpu6050_ai_test`，
确认链接器 Other flags 里有 `-L../NEAI -lneai`，`Build` 后把 `Debug/` 下的产物复制到本目录。
