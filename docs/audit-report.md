# 开源完整性 + "烧录成功但串口无输出" 审计报告

> 对象：`main` 分支快照（审计时对应 v3，即提交 `1ab6d0f`）
> 方法：逐文件核对 → 用 STM32CubeIDE 2.2.0 自带 GNU Tools for STM32 14.3.rel1
> 从源码原样重编 → 与仓库内预编译固件比对 → 代码路径分析
> 结论日期：2026-10-27　｜　写作时仓库状态：`main @ 0f2b0c1`（v4 固件 + 文档修正 + 许可证）；
> 此后发布的 **v4.1** 修正了 HSI 回退时的 TIM2 采样率（见第 5 节末），固件路径改为 `firmware/mpu6050_ai_test_v4.1.*`。

---

## 0. 先给结论

| # | 问题 | 结论 |
| --- | --- | --- |
| 1 | 开源是否完整？ | **基本完整**。源码、HAL、CMSIS、启动文件、链接脚本、AI 静态库、数据集、`.ioc`、预编译 hex 全在；从快照源码原样重编，334 个符号名/大小与仓库里的 v3 `.elf` **完全一致**，说明发布的固件就是这套源码编出来的。 |
| 2 | 是否少上传了东西？ | **只少一个：`led/` 点灯验证工程**。审计时 README 有两处引用它，但 `.gitignore:43-45` 明确写着"已从仓库移除，文件仍保留在本地"。它不影响主功能。 |
| 3 | 别人下载解压能否烧录实现功能？ | **能烧、能跑，但有前提**：传感器必须在上电时就在线；模型知识存在 RAM，每次上电都要重新 `r → l` 学习；固件**默认不主动刷数据**，要发命令（`h`/`s`/`r`/`d`）才有输出。 |
| 4 | 那位用户为什么串口没输出？ | **不是缺文件，是 v3 的启动顺序问题**：横幅和命令表被关在"MPU6050 初始化成功"里面。传感器没应答 → 只打印一行 `MPU6050 init FAIL`，之后所有命令都被忽略；如果串口助手是上电**之后**才打开的，连这一行都看不到 → 看起来就是"完全没反应"。此外 HSE 晶振起不来会静默死循环，也是零输出。v4 已把这两条路都堵掉。 |
| 5 | 最后的实时告警 LED 闪烁被删了吗？ | **没有被删**。v3 `collector.c:101-109` 里 `SIM < 80 → [ALARM] + PC13 闪 3 次` 还在，预编译 v3 hex 里也能检索到 `[ALARM] anomaly detected!` 字符串。真正"消失"的是 `led/` 那个点灯工程。 |

---

## 1. 验证证据（可复现）

### 1.1 源码能编、能链、能生成烧录文件

用 CubeIDE 2.2.0 自带的 `arm-none-eabi-gcc 14.3.1` 对工程原样编译
（`-mcpu=cortex-m3 -mthumb -O0 -g3`，`-DUSE_HAL_DRIVER -DSTM32F103xB`）：

```
text    data     bss
29524     92    8988     → bin 29,620 B
```

仓库 `firmware/mpu6050_ai_test.bin` 也是 **29,620 B**；
进一步用 `arm-none-eabi-nm --print-size` 比对两个 ELF 的 **334 个符号名称+大小，差异为 0**
（只有地址因链接顺序不同而整体平移）。→ **发布的 v3 固件与发布的源码同源，没有"源码和 hex 对不上"的问题。**

链接配置也已经在工程里（不是靠人肉记步骤）：
`.cproject:61` 和 `:137`（Debug/Release 两套配置）都有 `-L../NEAI -lneai`，
`NEAI/libneai.a`（41,258 B，24 个目标文件，Thumb-2 / CPU 7-M）符号齐全。

### 1.2 `.ioc` 是完整的，重新生成代码不会丢配置

`mpu6050_ai_test.ioc` 里已有：`PC13 = GPIO_Output`、`PA9=USART1_TX`、`PA10=USART1_RX`、
`PB6=I2C1_SCL`、`PB7=I2C1_SDA`、`TIM2`、`HSE 8MHz 晶振`、USART1/TIM2 中断使能。
→ 文档里"还要去 CubeMX 手动加 PC13"这类步骤其实是多余的（v3 文档 7.3 节，已修正）。

### 1.3 验证的边界

审计时**没有实物板**，所以"上板会不会出数据"属于**代码路径推断 + 产物核对**，不是实测。
下面凡是涉及硬件时序/电平的结论，都标了依据（代码行号）。

---

## 2. 完整性逐项核对

| 项目 | 状态 | 依据 |
| --- | --- | --- |
| CubeIDE 工程（`.cproject`/`.project`/`.mxproject`/`.launch`） | ✅ | 可导入、可编译（已实测重编） |
| `Core/Src`（main/collector/mpu6050/gpio/i2c/usart/tim/it/syscalls…） | ✅ | 12 个源文件齐全 |
| `Core/Inc`（含 `NanoEdgeAI.h`、`collector.h`、`mpu6050.h`、`stm32f1xx_hal_conf.h`） | ✅ | `NEAI_INPUT_SIGNAL_LENGTH 1`、`NEAI_INPUT_AXIS_NUMBER 3` |
| 启动文件 + 链接脚本 | ✅ | `startup_stm32f103c8tx.s`、`STM32F103C8TX_FLASH.ld` |
| HAL/CMSIS | ✅ | 与 `stm32f1xx_hal_conf.h` 打开的模块一一对应（无缺 .c 导致链接失败的问题） |
| AI 库 | ✅ | `NEAI/libneai.a`，`neai_anomalydetection_*` 全部导出 |
| 数据集 | ✅ | normal_01~05 / fault_01~05，各 1024 行 × 3 列 |
| 训练产物 | ✅ | `metadata.json`、`artifacts/`、`emulators/`（含 Windows/Linux 模拟器） |
| 预编译固件 | ✅ | `firmware/*.hex/.bin/.elf` 三种格式 |
| `.ioc` | ✅ | 见 1.2 |
| **`led/` 点灯工程** | ❌ **缺失** | README 引用，但 `.gitignore:43-45` 主动排除 |
| `firmware/` 在 README 目录树里的说明 | ❌ 缺失（已补） | v3 `README.md:78-112` 树里没有 firmware/ |

### 2.1 文档与代码不一致的地方（都已在后续提交中修正）

| 位置 | 问题 | 修正 |
| --- | --- | --- |
| `README.md:83-88`、`:140-142` | 目录树和 5.2 节引用 `led/`，仓库里没有 | 改为"`led/` 未上传，主固件开机自检代替" |
| `README.md:78-112` | 目录树没有 `firmware/` | 补上，并标 v4 推荐 |
| `README-第一步`:230 | 写"第二阶段引入定时器 **+ DMA**"，实际第二步固件没有 DMA（1ms 中断置标志 + 主循环读 I2C） | 改为 TIM2 1kHz 中断采样 |
| `README-第三步`:7.4 | 写"`main.c` **不用改**"，但本仓库 main.c 已经改成第三步版本；照文档沿用第二步 main.c 会漏掉初始化和回调 | 改为"main.c 也要用仓库版本"并说明原因 |
| `README-第三步`:7.3 | 让用户去 CubeMX 手动新增 PC13，实际 `.ioc` 里已有 | 保留但标注"仓库 .ioc 已配好" |

---

## 3. "烧录成功、串口没输出" 的代码级分析

把 v3 固件所有能走到"完全没输出"的路径列出来，并给出去分的方法。

### 路径 A（最可能）：MPU6050 没应答，横幅根本没打印

v3 `main.c:104-111`：

```c
if (MPU6050_Init(&hi2c1) == 1) {
    HAL_UART_Transmit(&huart1, "MPU6050 init OK\r\n", 17, 100);
    Collector_Init(&htim2, &huart1, &hi2c1);   /* ← 横幅/命令表/AI 初始化全在这里面 */
} else {
    HAL_UART_Transmit(&huart1, "MPU6050 init FAIL\r\n", 19, 100);
}
HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
```

`collector.c:112-134` 的 `Collector_Init()` 才打印
`=== MPU6050 AI Detector v3 ===`、命令表、`[AI] NanoEdge init OK`，也是在那里给
`collector.c` 的静态 `uart` 指针赋值。

所以传感器不应答时，实际发生的是：

1. 只打印一行 `MPU6050 init FAIL`（19 字节），然后不再有任何输出；
2. TIM2 没有启动（`HAL_TIM_Base_Start_IT` 在 `Collector_Init` 里）→ 采样根本不存在；
3. 串口接收中断照常开着，但 `Collector_HandleCmd()` 里的 `send_str()` 用的是**仍是 NULL 的 `uart`**，
   `HAL_UART_Transmit(NULL, ...)` 在句柄状态检查处直接返回 → 按 `h`/`s`/`r`/`d` 全部"没反应"；
4. 如果串口助手是在上电**之后**才打开的，那唯一一行 FAIL 也错过了 → 现象就是**彻底没输出**。

**怎么确认是这条**：看板载 LED。`gpio.c:54` 在 GPIO 初始化时就把 PC13 拉低（灯亮），
只有 `Collector_Init()`（`collector.c:121`）才把它拉高（灯灭）。
→ **LED 常亮 = 卡在传感器初始化**；LED 熄灭 = 主流程跑起来了。

常见的"传感器不应答"原因：SCL/SDA 接反、模块没供电（VCC 没接 3.3V）、
AD0 接了 VCC 变成 0x69、杜邦线接触不良、模块坏。

### 路径 B：HSE 8MHz 晶振没起振 → 静默死循环

v3 `main.c:91` 先配时钟，`main.c:147-150`：`HAL_RCC_OscConfig()` 失败 → `Error_Handler()`；
`main.c:191-200` 的 `Error_Handler()` 是 `__disable_irq(); while(1){}`。
此时 **USART 还没初始化**（`MX_USART1_UART_Init` 在后面）→ 永远一个字都没有，LED 也不亮
（GPIO 也还没初始化）。

**怎么确认**：LED 完全不亮、串口无输出、但 ST-Link 能连上芯片。多见于
没有 8MHz 晶振的克隆板、晶振虚焊/摔过的板子。v4 已改成"自动退回内部 HSI 64MHz 并打印提示"。

### 路径 C：串口线路/端口（和固件无关，但占现场问题的一半）

1. **TX/RX 必须交叉**：PA9 是 **TX**，要接 USB-TTL 的 **RXD**；PA10 接 TXD；
2. **必须共地**：USB-TTL 的 GND 和 Blue Pill 的 GND 要连；
3. **Blue Pill 的 USB 口不是串口**：插电脑不会出现 COM 口，必须用 CH340/CP2102 模块（或带 VCP 的 ST-Link）；
4. 波特率 115200 8N1、选对 COM 口（设备管理器里看）。

### 路径 D（v3 的第二个坑）：没学习就按 `d`，会误报告警

v3 `collector.c:94-99` 只判断了 `NEAI_NOT_INITIALIZED`，没判断
`NEAI_LEARNING_IN_PROGRESS`。此时 `sim` 仍是 0 → 打印 `SIM:0` 并触发 `[ALARM]`、灯闪 3 次。
用户会以为"模型/固件坏了"。v4 已补上状态判断（学习不足时只提示"多学几次"）。

---

## 4. 关于"实时告警的 LED 闪烁是不是被删了"

- **代码在**：v3 `collector.c:101-109`，`sim < ALARM_THRESHOLD(80)` 时打印
  `[ALARM] anomaly detected!` 并对 PC13 拉低/拉高各 100 ms，循环 3 次。
- **编进固件了**：`firmware/mpu6050_ai_test.hex` 里能检索到 `[ALARM] anomaly detected!`、
  `SIM:%u` 等字符串，说明这段确实在发布的二进制里。
- **仓库里消失的是 `led/`**：`.gitignore:43-45` 写着"早期板级点灯验证工程（Keil MDK），
  已从仓库移除，文件仍保留在本地"。记忆中的"LED 闪烁"很可能混了这两件事。
- **但 v3 的闪烁是阻塞的**：`HAL_Delay(100)` × 6 = 600 ms 卡在主循环里，这期间不采样、
  不响应串口。v4 改成 100 ms 状态机，由 1 ms tick 推进，不再打断检测。

---

## 5. v4 / v4.1：已发布并编译验证的版本

产物在 `firmware/mpu6050_ai_test_v4.1.{hex,bin,elf}`（**推荐替代 v3**；v3 已归档到
`firmware/legacy/`）。v4.1 = v4 + 下面最后一行那个修正。

| 改动 | 文件 | 解决什么 |
| --- | --- | --- |
| 开机先打印横幅 + `UART OK` + 时钟来源，再闪 3 下 LED 自检 | `main.c` | "完全没输出"变成"至少知道 MCU/串口活没活" |
| 传感器初始化 3 次重试，失败也继续跑命令行 | `main.c` | 传感器坏了不再整个 CLI 失效 |
| HSE 失败自动回退 HSI 64 MHz | `main.c` | 晶振坏的板子也能出串口 |
| I2C 全部改 50 ms 有限超时（不再 `HAL_MAX_DELAY`） | `mpu6050.c` | 总线卡死不再"假死" |
| 增加 `WHO_AM_I(0x75)` 校验（0x68 / 0x70~0x74 兼容） | `mpu6050.c` | 明确区分"有应答但芯片不对"和"没应答" |
| 新增 `MPU6050_BusRecover()`：9 个 SCL + STOP 解锁总线 | `mpu6050.c` | 掉电重启后 SDA 被拉死的常见故障 |
| 告警灯改非阻塞 100 ms 状态机 | `collector.c` | 闪灯不再卡采样/串口 |
| `detect()` 返回值全部处理 | `collector.c` | 学习不足时不再误报 ALARM |
| 空闲每 5 s 打印 `[alive]` 心跳 | `collector.c` | 空白屏幕也能判断"固件在跑" |
| `Error_Handler` 改 5 Hz 闪灯 | `main.c` | 启动失败也能用肉眼看见 |
| **v4.1**：HSI 回退时按实际 PCLK1 重算 TIM2 预分频 | `main.c` | 无晶振的板子也保持 1 kHz 采样（v4 会掉到约 889 Hz） |

编译结果（同一套工具链，可复现）：

```
v4   : text 32200  data 92  bss 8996  -> bin 32,296 B
v4.1 : text 32260  data 92  bss 8996  -> bin 32,356 B     Flash 49% / RAM 44%（20 KB）
```

v4.1 二进制与重新编译的产物 **SHA256 完全一致**（`8FB88AE4…`），说明 `firmware/` 里放的就是这套源码的产物。

> 注意：v4 改了 `Collector_Init()` 的签名（多了 `sensor_ok` 参数），
> 所以 `main.c` / `collector.c` / `collector.h` 必须**一起**替换。
> 只换其中一个会编译不过。

---

## 6. 给现场用户的回复（可直接复制粘贴）

> 你好，项目本身没问题，源码和固件都完整，我已经用官方工具链重新编译核对过。
> 你这个"烧录成功但串口没反应"，基本是固件启动顺序导致的，不是接线坏了。麻烦按下面做：
>
> 1. **先把串口助手打开**（115200、8N1、选 CH340 的 COM 口），**再按板子上的复位键**。
>    这个固件只在"上电那一下"打印菜单，之后就等命令了；先开助手后上电很关键。
> 2. 看**板载 LED（PC13）**：
>    - 上电闪 3 次后**常亮** → 单片机和串口是好的，是 MPU6050 没认到；
>    - **完全不亮** → 板子没供电/没烧进去/晶振问题；
>    - 常亮或闪烁但串口没字 → 串口线问题，继续看第 3 步。
> 3. 串口线检查：模块 **RXD 接 PA9（TX）**、**TXD 接 PA10（RX）**，**GND 必须共地**，
>    别用 Blue Pill 板载 USB 口当串口（它不是串口）。
> 4. 传感器检查：VCC 接 **3.3V**、GND、SCL→PB6、SDA→PB7、AD0 接 GND（地址 0x68）。
>    用万用表量一下模块 VCC 有没有 3.3V。
> 5. 打开串口助手后**按一下 `h` 回车**：能回帮助就说明串口双向都通；
>    能回 `[Sensor] MPU6050 not ready` 就说明是传感器那一段的问题。
> 6. 建议直接烧 `firmware/mpu6050_ai_test_v4.1.hex`：它开机就会打印
>    `UART OK` / `[CLK] ...` / `[BOOT] LED selftest done` / `[I2C] MPU6050 OK, WHO_AM_I=0x68`，
>    哪一步没过去一眼就能看出来；传感器没接好也会明确打印 `MPU6050 no answer (1/3)...`，
>    而且命令照常有回应，不会再"装死"。
> 7. 正常流程：烧录 → 上电 → 电机正常转 → 发 `r` 录 1 秒 → 发 `l` 学习 → 重复 3~5 次 →
>    发 `d` 开检测。注意模型知识在 RAM 里，**每次重新上电都要重新 `r → l`**。

---

## 7. 发布回顾 / 待办

- [x] `led/`：已从 README 删除引用（工程本身仍不上传）
- [x] v4 固件已随仓库发布（`firmware/*_v4.1.*`），README 已写清与 v3 的差别
- [x] README / `firmware/README.md` 已写明"先打开串口助手再上电"
- [x] `firmware/` 已有版本、体积、工具链信息
- [x] 许可证：MIT / Apache-2.0 / GPL-3.0 三选一（见 `LICENSE`）
- [ ] 可选：把 NanoEdge knowledge 存内部 Flash（`get_knowledge/set_knowledge`），免去每次上电重新学习
- [ ] 可选：把 `led/` 点灯工程补传（若想保留这条从零验证的学习路径）

---

## 8. 本文档对应的仓库改动

| 路径 | 内容 |
| --- | --- |
| `firmware/mpu6050_ai_test_v4.1.hex/.bin/.elf` | v4.1 固件（已编译、可复现，SHA256 与重编产物一致） |
| `firmware/README.md` | 烧录步骤、开机日志、LED 状态码表 |
| `.../Core/Src/{main,collector,mpu6050}.c`、`Core/Inc/{collector,mpu6050}.h` | v4 源码（提交 `300d756`） |
| `README.md`、第一/三步文档 | 文档修正（提交 `9be15a0`） |
| `LICENSE`、`LICENSE-MIT`、`LICENSE-APACHE`、`LICENSE-GPL-3.0` | 三选一许可证（提交 `0f2b0c1`） |

源码改动可直接用 `git show 300d756` 查看，不再单独提供 patch 文件。

---

## 9. 未验证项与风险

1. **没有实物板**：v4/v4.1 的串口/LED 实际表现是按代码推断的，未上板实测；
   已验证的是"能编译、体积不超、二进制可复现、符号与 v3 完全一致"。
2. HSI 回退后波特率：64 MHz 下 USART1 115200 的 BRR 误差约 −0.08%，远小于 UART 容限（~2%），可用。
3. `metadata.json` 显示 AI 库编译时带 `fshort_enums` / soft-float；当前工程未加 `-fshort-enums`。
   本项目 ABI 只用"返回 enum + 传 `float*`/`uint8_t*`"，链接与运行逻辑不受影响；
   但以后若把 enum 放进结构体跨库传递，需要补 `-fshort-enums`。
4. v4 的 `SystemClock_Config()` 加了 HSI 回退，**CubeMX 重新生成代码会覆盖它**；
   改 `.ioc` 重新生成后要按 `main.c` 里的注释补回来。
5. 本报告的结论基于审计当时的快照；后续若改动固件或文档，请以新版本为准。
