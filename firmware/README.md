# 固件（可直接烧录）

本目录是 `project1/stm32cubeide_test/mpu6050_test/mpu6050_ai_test/` 编译出的产物，
方便不装 IDE 也能直接烧板验证。三个文件内容等价，按工具选用其一：

| 文件 | 大小 | 用途 |
| --- | --- | --- |
| `mpu6050_ai_test.elf` | 900 KB | 带符号与调试信息，CubeIDE / GDB 调试用（含源码行号） |
| `mpu6050_ai_test.hex` | 81 KB | Intel HEX，STM32CubeProgrammer、ST-Link Utility、Keil 都能直接下载 |
| `mpu6050_ai_test.bin` | 29 KB | 裸二进制，只能烧到 `0x08000000` 起始地址 |

## 构建信息

| 项 | 值 |
| --- | --- |
| 目标芯片 | STM32F103C8Tx（Cortex-M3，64 KB Flash / 20 KB RAM） |
| 构建工具 | STM32CubeIDE 2.2.0 + GNU Tools for STM32 14.3.rel1（arm-none-eabi-gcc） |
| 架构 | `elf32-littlearm`，armv7，入口 `0x08001b15` |
| Flash 占用 | **29,616 B / 65,536 B ≈ 45%**（text 29,524 + data 92） |
| RAM 占用 | **9,080 B / 20,480 B ≈ 44%**（data 92 + bss 8,988） |
| 对应源码 | 仓库 `main` 分支，`collector.c` 含「跳过 x,y,z 表头」提示语修正之后的版本 |

其中 RAM 的大头是采集缓冲 `int16_t buf[1024][3]`（6 KB）加 NanoEdge 库运行时开销。

## 烧录

**STM32CubeProgrammer**（推荐，支持 hex/bin/elf 全格式）：

1. ST-Link 连板子（SWDIO/PA13、SWCLK/PA14、3.3V、GND）；
2. 打开 CubeProgrammer → 选 ST-LINK → Connect；
3. 打开 `mpu6050_ai_test.hex` → **Download**；
4. 断电重上电，串口（115200 8N1）应出现菜单。

**STM32CubeIDE**：`Run → Run Configurations` 里把固件指向这个 `.elf` 也能下载。

## 上板后怎么用

```
=== MPU6050 AI Detector v3 ===
Cmd: s=stream r=record l=learn d=detect c=cancel h=help
[AI] NanoEdge init OK, signal len=1
```

- `r` 录 1 秒窗口 → `l` 学习（正常工况下重复 3~5 次）
- `d` 开关连续检测：正常 `SIM:80+`，加故障后相似度下降并打印 `[ALARM]`，PC13 LED 闪 3 次

> ⚠️ 模型知识存在 **RAM**，掉电即失。每次重新上电都要重新执行 `r → l` 学习流程。
> 详细说明见 [第三步文档](../project1/README-第三步-NanoEdge训练与板上部署.md)。

## 重新构建

用 CubeIDE 打开 `project1/stm32cubeide_test/mpu6050_test/mpu6050_ai_test`，
确认链接器 Other flags 里有 `-L../NEAI -lneai`，`Build` 后把 `Debug/` 下的产物复制到本目录。
