# 第三步：NanoEdge AI Studio 训练 + 板上实时异常检测

> 目标：用第二步采集的 normal /fault 数据集，在 NanoEdge AI Studio 里训练出
> 异常检测模型，生成 AI 静态库（libneai.a + NanoEdgeAI.h），集成进 CubeIDE 工程，
> 实现 "板上学习正常态 → 实时检测 → 相似度告警"。



***

## 0. 前提



* 数据集就绪：`normal_XX.csv` / `fault_XX.csv`，每个文件 **1024 行 × 3 列、无表头**；本仓库 `project1/NanoEdge ai_math/project_1/` 已含 normal / fault 各 5 个（跑通流程够用），建议扩到每类 15~20 个再正式训练；

* 第二步的工程能正常录制 CSV（`r` 命令 2.5 秒出块）；

* 已注册 myST 账户（ST 官网免费）。



***

## 1. 安装 NanoEdge AI Studio（免费）



1. 打开 ST 官网 NanoEdge AI Studio 页面（`st.com` 搜索 NanoEdgeAIStudio，或 `stm32ai.st.com/nanoedge-ai`）；

2. 下载对应系统（Windows）安装包，用 myST 账户登录安装；

3. 安装后启动，登录 myST 账户即可使用（完全免费，支持所有 STM32 Cortex-M 目标）。

> 说明：NanoEdge AI Studio 现在是 ST Edge AI Suite 的一部分，免费、含商业用途。



***

## 2. 创建异常检测工程



1. 启动 Studio，点击 **New Project**；

2. 工程类型选 **Anomaly detection**（异常检测）；

3. Project settings：

* 工程名随意，如 `motor_vibration_ad`；

* **Target**：选 **Microcontroller** 类型 → 搜索 **STM32F103** → 选 **STM32F103C8Tx**（找不到 C8 就选任意 F103 Cortex-M3 型号，库是通用的）；

* 保存进入下一步。



***

## 3. 导入数据集（Signals / Data 界面）



1. 在数据界面，添加 **Nominal data（正常数据）**：把 `normal_01.csv ~ normal_05.csv`（或你自己采集的全部文件）拖入或逐个添加；

2. 添加 **Abnormal data（异常数据）**：把 `fault_01.csv ~ fault_05.csv`（或你自己采集的全部文件）添加进去；

3. 每个文件 = 1 个信号（1024 样本 × 3 轴），Studio 会自动识别 3 列 = 3 个轴；

4. **检查**：确认每个信号的行数正确（**1024 数据行**）。本仓库的 CSV 无表头，导入后应正好 1024 行；如果你自己保存时把 `x,y,z` 表头也复制进去了，会变成 1025 行，用 **Data Manipulation** 工具删掉第一行：

* 导入文件 → **Extract lines**：起始行 1，结束行按实际 → 或直接选 "从第 1 行开始删 1 行"；

* 或者最简单：手动把 CSV 的表头行删掉后再导入。

> 提示：我们的采样率是 1kHz，窗口 1024 样本 = 1 秒信号。Studio 不强制填采样率（那是 Sampling Finder 用的），直接用现有窗口即可。



***

## 4. Benchmark（自动找最优模型）



1. 点击 **Benchmark**，等待运行（几分钟到几十分钟，取决于数据量）；

2. 结果表会列出候选算法的 **Accuracy / RAM / Flash / Signal length**：

* 优先选 **Accuracy 高（95%+）** 且 **RAM/Flash 适合 STM32F103C8（RAM 20KB、Flash 64KB）** 的结果；

* 记下该结果的 **Signal length**（输入信号长度，如 128/256/512/1024）；

1. 如果所有结果的 RAM 都超过 20KB 预算，说明信号太长 —— 用 Data Manipulation 把窗口抽短（例如从 1024 行抽到 256 行：**Extract lines / Change Column Number** 处理）后重新 Benchmark。

> 本仓库实际选中的结果：**MML** 算法（预处理 = 绝对值 + 32% 移动平均池化），
> signal length = **1 × 3 轴**，`learning_min_lines = 56`，预计 **RAM 624 B / Flash 564 B**，
> 远低于 F103C8 的 20 KB / 64 KB —— 不需要抽短窗口。详见 `libneai_nanoedge-ai-test-try1_2/metadata.json`。



***

## 5. Validation（可选但建议）

在 **Validation** 界面用库的模拟器（Emulator）跑一遍 nominal 和 abnormal 数据：



* nominal 数据相似度应接近 100；

* abnormal 数据相似度应明显低于正常值（通常 < 90 甚至 < 50）；

* 如果区分不开，回到数据 / 参数重新 Benchmark 或检查数据质量（传感器贴牢、工况区分明显）。



***

## 6. Compile → 下载 AI 库



1. 在 **Deployment** 界面选择你选中的 Benchmark 结果；

2. 点击 **Compile**，等待编译；

3. 下载得到的 `.zip`，里面有：

* `libneai.a`（模型静态库）

* `NanoEdgeAI.h`（接口头文件，含 `NEAI_INPUT_SIGNAL_LENGTH` / `NEAI_INPUT_AXIS_NUMBER` 宏和函数声明）

* Emulator、metadata.json（用不上可不管）



***

## 7. CubeIDE 集成

### 7.1 放文件



* 在工程根目录新建文件夹 `NEAI/`：


  * 把 `libneai.a` 放进去；

  * 把 `NanoEdgeAI.h` 复制到 `Core/Inc/`；

* 右键工程 → **Refresh**。

### 7.2 链接库（关键步骤）

> ⚠️ 注意：CubeIDE 的编译工作在
> `Debug`
> 子目录里执行，相对路径是从
> `Debug`
> 算起的，所以填
> `NEAI`
> 会指向不存在的
> `Debug\NEAI`
> （报
> `cannot find -lneai`
> ）。
> 推荐直接用下面的方法绕开：

`Project → Properties → C/C++ Build → Settings → Tool Settings → MCU/MPU GCC Linker → Libraries`：



* 把 **Libraries (-l)** 和 **Library search path (-L)** 两个列表里的条目全部删除；

* 同一 Linker 下点 **Miscellaneous → Other flags（其他标志）**，填入：



```
-L../NEAI -lneai
```

（`..` = 从 Debug 回到工程根目录，再进 NEAI。若仍报错，改用绝对路径：

`-LD:/你的路径/mpu6050_ai_test/NEAI -lneai`）

### 7.3 CubeMX 新增 PC13



* 打开 `.ioc` → `PC13` 设为 **GPIO\_Output**（板载 LED 用）→ GENERATE CODE；

* （USER CODE 区代码会自动保留，重新生成不影响已写的代码）

### 7.4 替换采集器代码（拿到库之后再做）



* 用本仓库的 `collector.c` / `collector.h` **覆盖**工程里的同名文件（Refresh）；

* **`main.c` 也要用本仓库的版本**：v4 起 `Collector_Init()` 多了一个 `sensor_ok` 参数，
  并且增加了开机自检、HSE→HSI 回退和传感器重试。继续用第二步的 main.c 会编译不过；
  若沿用 v3 的 main.c，则传感器一失败串口就完全没有输出（现场很难排查）。

* **Build → Run** 烧录。此时上电日志变为：



```
=== MPU6050 AI Detector v4.1 ===
UART OK: USART1 PA9(TX)/PA10(RX) 115200 8N1
[CLK] HSE 8MHz -> PLL 72MHz
[BOOT] LED selftest done (PC13), init MPU6050...
[I2C] MPU6050 OK, WHO_AM_I=0x68
...
[AI] NanoEdge init OK, signal len=1
[ready] type h for help
```

`signal len` 显示的是模型要求的输入长度——本仓库选中的模型是 **1 样本 × 3 轴**，所以显示 `1`（换成别的 Benchmark 结果这个数会变）。学习 / 检测都按这个长度切窗口：`collector.c` 会把 1024 样本的录制窗口从头到尾切成若干个子窗口逐个喂给库。



***

## 8. 板上部署测试（核心演示流程）

### 8.1 学习正常态（关键步骤）



1. 电机正常运转、稳定后，发 `r` 录一个正常窗口（约 1 秒）；

2. 发 `l` 学习这个窗口（内部自动切成 signal len 大小的子窗口逐个学习）；

3. **重复「r → l」共 3\~5 次**，让模型充分学习正常态的振动特征；

4. 每次学习会打印 `[AI] learned N signal(s)`。

> 注意：学习用的数据
> **必须是正常工况**
> （电机正常转），否则模型会把故障当正常。

### 8.2 连续检测



1. 发 `d` 开启检测模式：板子自动循环「录 1 秒窗口 → detect → 打印相似度」；

2. 电机正常运转时，应看到 `SIM:80+`（越接近 100 越正常）；

3. **给电机加故障**（贴配重 / 卡异物）后，相似度应明显下降：

* `SIM < 80` → 打印 `[ALARM] anomaly detected! (PC13 LED blinks 3x)`，
  板载 LED 闪 3 次（v4 起为非阻塞状态机，闪烁期间检测/串口都不受影响）；

1. 再发 `d` 关闭检测模式。

### 8.3 阈值调整

相似度阈值在 `collector.c` 顶部 `ALARM_THRESHOLD`（**默认 80**，ST 推荐的起点也是 80；判据是 `sim < ALARM_THRESHOLD` 即告警）。按实测：



* 正常 SIM 常低于 80 → 把阈值调低（如 70），减少误报；

* 故障 SIM 仍高于阈值 → 调高（如 90）或重新学习 / 换 Benchmark 结果。



***

## 9. 常见问题



| 现象                                | 处理                                                                                                                |
| --------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| 编译报错找不到 neai / 链接失败               | 检查 7.2 的 `-L../NEAI -lneai` 是否填在 **Linker → Miscellaneous → Other flags** 里，且 `NEAI/` 下确实有 `libneai.a`；**不要**再往 Libraries(-l) / search path(-L) 里加一遍（那两处要留空）                                                              |
| RAM 超限（20KB）                      | Studio 里 Benchmark 选 RAM 更小的结果，或用 Data Manipulation 把信号抽短（如 256 样本），相应调整 COLLECT\_SAMPLES                         |
| `init FAIL`                       | 库没链接好（看编译是否成功），或 target 选错                                                                                        |
| 检测前没学习，detect 返回 NOT\_INITIALIZED | 先做 8.1 的学习流程                                                                                                      |
| 正常 / 故障 SIM 区分不开                  | 数据质量：传感器贴牢、工况稳定再录；增加学习窗口数；重新 Benchmark 换算法                                                                        |
| 掉电后知识丢失                           | AD 库的知识存在 RAM，掉电即失。重启后重新执行 8.1 学习流程即可（进阶：用 `neai_anomalydetection_get_knowledge/set_knowledge` 存到内部 Flash，重启自动恢复） |



***

## 10. 参考资料



* ST Wiki：NanoEdge AI Library for anomaly detection（API、链接方法、阈值建议）

* ST Wiki：How to Build an Anomaly Detection Project for Predictive Maintenance with NanoEdge AI Studio（完整教程，本方案参照它设计）