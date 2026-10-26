# 第一步：STM32F103C8T6 + GY-521 (MPU6050) 串口读取振动数据

> 目标：把 GY-521 的三轴加速度数据通过串口发到电脑，用串口助手看到数据 "正常跳动"，
> 验证传感器、接线和工程框架都 OK。本步骤无需 OLED / 电机 / 蜂鸣器。



***

## 0. 需要的硬件



| 器件                        | 说明                                  | 参考价    |
| ------------------------- | ----------------------------------- | ------ |
| STM32F103C8T6 Blue Pill   | 主控板                                 | \~10 元 |
| ST-Link V2                | SWD 下载 / 烧录（建议带 VCP 的，可省一个 USB-TTL） | \~15 元 |
| GY-521 (MPU6050)          | 三轴加速度计模块（你已有）                       | \~5 元  |
| USB-TTL 模块 (CH340/CP2102) | **读串口数据必需**：Blue Pill 板载 USB 不是串口桥  | \~5 元  |
| 杜邦线若干                     | 母对母为主                               | —      |

> 注意：如果 ST-Link 是带 VCP（虚拟串口）的型号，可以用它的串口通道直接看数据，USB-TTL 可以省掉。



***

## 1. 接线

### GY-521 ↔ Blue Pill



| GY-521 | Blue Pill       |
| ------ | --------------- |
| VCC    | 3.3V            |
| GND    | GND             |
| SCL    | PB6 (I2C1\_SCL) |
| SDA    | PB7 (I2C1\_SDA) |

### ST-Link V2 ↔ Blue Pill（SWD，4 根）



| ST-Link | Blue Pill  |
| ------- | ---------- |
| 3.3V    | 3.3V       |
| GND     | GND        |
| SWDIO   | DIO (PA13) |
| SWCLK   | CLK (PA14) |

### USB-TTL ↔ Blue Pill（读串口用）



| USB-TTL | Blue Pill         |
| ------- | ----------------- |
| RXD     | PA9 (USART1\_TX)  |
| TXD     | PA10 (USART1\_RX) |
| GND     | GND（**必须与主板共地**）  |

> 常见错误：RXD/TXD 接反、忘记共地、SCL/SDA 接反。



***

## 2. CubeMX 工程配置（约 10 分钟）



1. **新建工程**：`File → New Project` → 芯片搜索 `STM32F103C8Tx`（选 `STM32F103C8Tx`，带后缀 F 的别选）→ `Start Project`。

2. **SYS → Debug = Serial Wire** —— 最关键的一步，不开的话烧录一次后就没法再下载（需要 BOOT0 救砖）。

3. **RCC → High Speed Clock (HSE) = Crystal/Ceramic Resonator**（Blue Pill 板载 8MHz 晶振）。

4. **Clock Configuration**：输入 `HCLK = 72`，回车让工具自动配好（8MHz × 9 = 72MHz）。

5. **I2C1**：勾选 Enabled，引脚自动落到 PB6/PB7；`Parameter Settings → I2C Speed Mode = Fast Mode (400KHz)`（若通讯不稳，改回 Standard Mode 100KHz）。

6. **USART1**：Mode = Asynchronous，`Parameter Settings → Baud Rate = 115200`（PA9/PA10 自动分配）。

7. **Project Manager**：Project Name 随便（如 `mpu6050_demo`）；Toolchain = `STM32CubeIDE`，勾选 `Copy only the necessary library files`；点右上角 **GENERATE CODE** 生成工程。



***

## 3. 把驱动文件放进工程



* `mpu6050.c` → 复制到工程 `Core/Src/` 目录

* `mpu6050.h` → 复制到工程 `Core/Inc/` 目录

* （在 CubeIDE 里右键工程名 → `Refresh`，让文件出现在工程树中）



***

## 4. 修改 main.c（按标记区粘贴）

### 4.1 文件开头（`USER CODE BEGIN Includes` 与 `USER CODE END Includes` 之间）



```
/* USER CODE BEGIN Includes */
#include "mpu6050.h"
#include "stdio.h"    /* sprintf 的声明在这 */
#include "string.h"   /* strlen 的声明在这 */
/* USER CODE END Includes */
```

### 4.2 全局变量区（`USER CODE BEGIN PV` 与 `USER CODE END PV` 之间）



```
/* USER CODE BEGIN PV */
MPU6050_Accel_t accel;
char print_buf[64];
/* USER CODE END PV */
```

### 4.3 初始化（`USER CODE BEGIN 2` 与 `USER CODE END 2` 之间）



```
/* USER CODE BEGIN 2 */
if (MPU6050_Init(&hi2c1) == 1) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"MPU6050 init OK\r\n", 17, 100);
} else {
    HAL_UART_Transmit(&huart1, (uint8_t*)"MPU6050 init FAIL\r\n", 19, 100);
}
/* USER CODE END 2 */
```

### 4.4 主循环（`USER CODE BEGIN WHILE` 与 `USER CODE END WHILE` 之间）



```
/* USER CODE BEGIN WHILE */
while (1)
{
    if (MPU6050_Read_Accel(&hi2c1, &accel) == 1)
    {
        sprintf(print_buf, "AX:%d AY:%d AZ:%d\r\n", accel.x, accel.y, accel.z);
        HAL_UART_Transmit(&huart1, (uint8_t*)print_buf, strlen(print_buf), 100);
    }
    HAL_Delay(10);   /* 10ms 一帧，约 100Hz 刷新率 */
    /* USER CODE END WHILE */
}
```

> 说明：用 
>
> `sprintf`
>
>  \+ 
>
> `HAL_UART_Transmit`
>
> ，不需要配置 MicroLIB /printf 重定向；
> 数据用整型原始值（±2g 量程下 1g = 16384），避免浮点打印的坑。



***

## 5. 编译与烧录



1. CubeIDE 中点击锤子图标 **Build**（或用 `Project → Build All`），底部 `0 errors`。

2. 点绿色箭头 **Run**（或 `Run → Debug`，第一次选 ST-Link 调试器）。

3. 烧录成功后板子开始运行，串口马上有输出。



***

## 6. 上位机验证（预期现象）



1. 打开设备管理器确认端口号（USB-TTL 对应 CH340/CP2102 的 COM 口）。

2. 打开串口助手（XCOM / SSCOM 均可）：波特率 **115200**、数据位 8、停止位 1、无校验。

3. 观察：

* 模块**平放桌面**：`AX≈0、AY≈0、AZ≈16000±200`（重力 1g 落在 Z 轴）；

* **用手晃动 / 敲一下桌子**：三个值明显跳变；

* 数据约 100 行 / 秒 持续刷新。

**只要看到数值随晃动跳变，第一步就通过了** —— 传感器、I2C、串口、工程框架全部验证 OK。



***

## 7. 常见问题排查



| 现象                  | 原因与处理                                                                |
| ------------------- | -------------------------------------------------------------------- |
| 烧录后板子无反应 / 无法再次下载   | SWD 没开 → BOOT0 拉高再上电，可重新烧录；随后把 SYS→Debug 改为 Serial Wire              |
| 串口无任何输出             | 先看 USB-TTL 是否共地；RXD/TXD 是否接反；波特率是否 115200                            |
| 输出 `init FAIL` 或全 0 | I2C 通讯失败：查 SCL/SDA 是否接反、VCC/GND、地址是否为 0x68；HAL 硬件 I2C 卡死 → 换软件模拟 I2C |
| 输出乱码                | 波特率不一致 / 串口被别的软件占用                                                   |
| 数值恒定不变              | I2C 读取失败（读到默认值），优先排查共地与地址                                            |



***

## 8. 下一步预告

第一步通过后，第二阶段再引入**TIM2 定时 1 kHz 中断采样**（不是 DMA，仓库里的
第二步固件就是 1 ms 中断置标志、主循环读 I2C）、把数据打标导出 CSV，

然后进入 NanoEdge AI Studio 训练异常检测模型。到时继续帮你配。