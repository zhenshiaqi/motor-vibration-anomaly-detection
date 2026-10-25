# 第二步：定时器固定频率采样 + 窗口录制 + CSV 数据采集

> 目标：用 TIM2 定时器实现
> **1kHz 固定频率采样**
> ，通过串口命令触发录制一个
> 1024 样本 × 3 轴的振动数据窗口，按 CSV 块输出到电脑，保存为带标签的数据文件，
> 为 NanoEdge AI Studio 训练异常检测模型准备数据集。



***

## 0. 硬件



* 与第一步完全一样（Blue Pill + ST-Link + GY-521 + USB-TTL），**无需新增**。

* 建议尽快入手一个**小直流电机或 USB 风扇**（5\~15 元）作为真实振动源：

  用双面胶把 GY-521 贴在电机 / 风扇外壳上，电机运转 = 正常工况，人为加故障 = 异常工况。

> 没有电机时也可以先用 "静止桌面 vs 用手晃动 / 敲击" 验证整套采集流程，
> 但真正训练用的数据建议等电机到位后再采。



***

## 0.1 电机怎么转起来（两种方式）

> 先明确一点：
> **采集数据这一步，STM32 不需要控制电机**
> 。电机只要自己稳定运转，
> STM32 负责采样振动即可。以下两种方式任选。

### 方式 A：电机独立供电（最简单，推荐先用这个）



1. 电机 / 风扇用独立的 5V 电源（手机充电器、充电宝、电池盒）直接供电，让它持续稳定运转；

2. 用双面胶 / 热熔胶把 GY-521 **牢固**贴在电机外壳上（贴紧，别用软泡沫、别让模块悬空晃动）；

3. 电机与 STM32 **不需要任何电气连接**（传感器靠机械振动采集信号），各转各的；

4. 等电机运转稳定 2\~3 秒后，再发 `r` 录制窗口。

* 优点：零额外硬件、立刻能采数据；电机电流波动不会干扰传感器供电。

* 缺点：无法程序控制转速（后续要做调速实验时再升级成方式 B）。

### 方式 B：STM32 控制电机（为后续调速预留）

**不能把电机直接接到 GPIO**（引脚电流只有 20mA，电机一启动就会烧引脚），需要驱动电路，二选一：



1. **MOSFET / 三极管方案（最便宜，约 2 元）**：N-MOSFET（AO3400）或 NPN 三极管（S8050）+ 1N4007 续流二极管 + 100Ω 门极电阻：

* 电机正极 → 5V 电源正

* 电机负极 → MOSFET 漏极（D）

* MOSFET 源极（S）→ GND（**必须与 STM32 共地**）

* 1N4007 反向并联在电机两端（阴极接电机正极，防反电动势）

* MOSFET 栅极（G）→ 100Ω → STM32 **PA6**

1. **驱动模块方案（省事，约 10\~15 元）**：买 L298N 或 TB6612 模块，IN1/IN2 接两个 GPIO，模块电源接 5V，输出接电机。

代码方面：CubeMX 里把 **PA6 设为 GPIO\_Output**（重新生成），主循环外加：

`HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);` 电机全速转，`GPIO_PIN_RESET` 停。

> 无论哪种方式，都建议电机用
> **独立 5V 电源**
> （不要和 GY-521 共用板载 3.3V）：
> 电机启停的电流冲击会污染传感器电源，让振动数据掺进电噪声，影响后面训练效果。



***

## 1. CubeMX 新增配置（在第一步工程上改）



1. 打开工程 → **TIM2**：`Activated`，Parameter Settings：

* Prescaler = **71**

* Counter Period = **999**

* （定时器时钟 72MHz ÷ 72 = 1MHz，计数 1000 次 = 1ms → 1kHz 中断）

1. **TIM2 NVIC**：勾选 `TIM2 global interrupt`。

2. **USART1 NVIC**：勾选 `USART1 global interrupt`（用于接收串口命令）。

3. `GENERATE CODE` 重新生成。



***

## 2. 把文件放进工程



* `collector.c` → `Core/Src/`

* `collector.h` → `Core/Inc/`

* CubeIDE 里右键工程名 → `Refresh`



***

## 3. 修改 main.c（5 处）

### 3.1 Includes（USER CODE BEGIN Includes）



```
/* USER CODE BEGIN Includes */
#include "mpu6050.h"
#include "collector.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */
```

### 3.2 全局变量（USER CODE BEGIN PV）



```
/* USER CODE BEGIN PV */
uint8_t rx_byte;   /* 串口命令接收字节 */
/* USER CODE END PV */
```

> 注意：第一步的
> `MPU6050_Accel_t accel; char print_buf[64];`
> 可以删掉，
> 现在采集由 collector.c 内部管理。

### 3.3 初始化（USER CODE BEGIN 2）



```
/* USER CODE BEGIN 2 */
if (MPU6050_Init(&hi2c1) == 1) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"MPU6050 init OK\r\n", 17, 100);
    Collector_Init(&htim2, &huart1, &hi2c1);        /* 启动定时采样 */
} else {
    HAL_UART_Transmit(&huart1, (uint8_t*)"MPU6050 init FAIL\r\n", 19, 100);
}
HAL_UART_Receive_IT(&huart1, &rx_byte, 1);          /* 命令接收不依赖传感器是否正常 */
/* USER CODE END 2 */
```

### 3.4 主循环（USER CODE BEGIN WHILE）—— 把第一步的轮询代码整个替换掉



```
/* USER CODE BEGIN WHILE */
while (1)
{
    Collector_Process();   /* 采样/录制/输出状态机 */
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
}
/* USER CODE END 3 */
```

### 3.5 回调函数（USER CODE BEGIN 4 与 USER CODE END 4 之间）



```
/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        Collector_OnTick();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        Collector_HandleCmd(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  /* 继续接收下一个命令 */
    }
}
/* USER CODE END 4 */
```



***

## 4. 编译烧录

Build（0 errors）→ Run。复位后串口应打印：



```
=== MPU6050 Data Collector v2 ===
Cmd: s=stream  r=record  c=cancel  h=help
```



***

## 5. 串口助手设置（重要）



* 波特率 115200，8N1；

* **取消勾选 "HEX 显示"**；

* 关闭 "时间戳显示"（否则会把时间戳混进 CSV）；

* 发送区输入命令后点**发送**（或勾选 "发送新行 / 加回车换行"）。



***

## 6. 采集流程（打标规范）

### 6.1 实时流测试（可选）

发 `s` 开始 100Hz 实时打印，再发 `s` 停止，确认数据正常。

### 6.2 录制正常工况（normal）



1. 电机正常运转（模块贴在外壳上）；

2. 发 `r`，等待约 1 秒 + 输出时间（1024 行 ≈ 1.3 秒）；

3. 接收区出现：



```
====BEGIN====
x,y,z
16384,0,0
16382,1,-1
...
====END====
[Rec] done, copy 1024 data lines between BEGIN/END (skip x,y,z)
```



1. 只选中**数据行**（第一个数据行到最后一个，共 **1024 行**）复制——不要 `====BEGIN====` / `====END====`，也**不要**把 `x,y,z` 表头行带上（否则 NanoEdge 会把它当成第 1025 行数据，导入后还得删）；

2. 新建文本文件保存为 `normal_01.csv`，再录一个存 `normal_02.csv`…… 共 **10\~20 个**。

### 6.3 录制故障工况（fault）



1. 制造故障：在电机 / 风扇叶片上**贴一小块配重（胶带 / 橡皮泥）**，或卡一片纸条改变负载；

2. 发 `r`，按同样方法保存为 `fault_01.csv`、`fault_02.csv`…… 共 **10\~20 个**。

### 6.4 文件组织



```
dataset/
├── normal_01.csv ... normal_15.csv     # 正常/健康工况
└── fault_01.csv  ... fault_15.csv      # 故障工况
```

每个文件 = 一个信号窗口（1024 行 × 3 列），这就是 NanoEdge 需要的 "信号" 粒度。

> 本仓库实际把采集到的数据放在 `project1/NanoEdge ai_math/project_1/`，目前 normal / fault 各 5 个（跑通流程够用，正式训练建议每类补到 15~20 个）。

录制时**每种工况之间要让电机恢复稳定状态再发&#x20;**`r`，避免把上一工况的振动带进窗口。



***

## 7. 参数怎么调（可选）



* **窗口更长**：改 `collector.h` 里 `COLLECT_SAMPLES`（如 2048 = 2 秒），

  同时注意 RAM：2048×3×2B = 12KB，20KB 内可用；

* **采样率不同**：改 TIM2 的 Prescaler/Period（如 2kHz：PSC=71，ARR=499）。

  后续 NanoEdge 的 Sampling Finder 会给出推荐值，到时候按它调。



***

## 8. 常见问题



| 现象                     | 处理                                                          |
| ---------------------- | ----------------------------------------------------------- |
| 复位后没有菜单                | 检查 main.c 3.3/3.4/3.5 是否都改了；Collector\_Init 只在 init OK 后才调用 |
| 发 `r` 后一直没输出           | 先发 `s` 看实时流是否正常；I2C 读取失败时录制不推进，检查接线                         |
| 录出来的 CSV 里混有 `[录]` 等提示 | 复制时只选纯数据行（不含 BEGIN/END 与 `x,y,z` 表头）   
| 串口发命令无效                | 确认 USART1 NVIC 已勾选；命令要勾选 "发送新行"                             |



***

## 9. 下一步预告

数据集准备完成后，进入第三步：**把 CSV 导入 NanoEdge AI Studio**

（Data Manipulation 工具 → Benchmark 选最优模型 → 生成 libneai.a），

再回到 CubeIDE 把 AI 库集成进这个工程，实现板上实时异常检测。到时继续帮你配。