/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "mpu6050.h"
#include "string.h"
#include "stdio.h"
#include "collector.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 开机先自检多久（ms）：给串口助手/上位机一点时间，别让第一行字丢在半路 */
#define BOOT_SETTLE_MS      200U
/* 传感器初始化尝试次数 */
#define SENSOR_INIT_TRIES   3U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

uint8_t rx_byte;                /* 串口命令接收字节 */
uint8_t g_sensor_ok = 0;        /* MPU6050 是否在线（1=在线） */
uint8_t g_clk_hsi = 0;          /* 1 = HSE 晶振没起来，已退回内部 HSI 64MHz */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 板载 LED：PC13，低电平点亮 */
static void led_write(GPIO_PinState state)
{
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, state);
}

/* 串口打印一行（不依赖 collector，开机早期就能用） */
static void say(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, strlen(s), 200);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  /* ============================================================
   * 开机自检（v4 核心改动）
   *
   * v3 的打印全部藏在 Collector_Init() 里，而 Collector_Init() 只有
   * MPU6050_Init() 成功时才会被调用 —— 传感器没接好，板子就"一声不吭"，
   * 用户没法区分"固件没跑"、"串口接错"和"传感器坏了"。v4 按下面顺序自检：
   *   1) 先证明 MCU + 时钟 + 串口活着（打印 + LED 闪 3 次）；
   *   2) 再初始化传感器，失败也照常进入命令循环，并明确打印原因；
   *   3) 传感器失败时 LED 保持常亮，给一个不用串口也能看的信号。
   * ============================================================ */
  HAL_Delay(BOOT_SETTLE_MS);

  say("\r\n=== MPU6050 AI Detector v4 ===\r\n");
  say("UART OK: USART1 PA9(TX)/PA10(RX) 115200 8N1\r\n");
  if (g_clk_hsi) {
      say("[CLK] HSE/8MHz crystal failed -> fallback to HSI 64MHz\r\n");
  } else {
      say("[CLK] HSE 8MHz -> PLL 72MHz\r\n");
  }

  /* LED 自检：闪 3 次。看到这 3 下，就说明 MCU 跑起来了、GPIO 没问题。 */
  for (uint8_t i = 0; i < 3U; i++) {
      led_write(GPIO_PIN_RESET);
      HAL_Delay(80);
      led_write(GPIO_PIN_SET);
      HAL_Delay(120);
  }
  say("[BOOT] LED selftest done (PC13), init MPU6050...\r\n");

  /* ---- 传感器初始化：最多试 3 次，失败之间做一次 I2C 总线自救 ---- */
  g_sensor_ok = 0;
  for (uint8_t attempt = 1U; attempt <= SENSOR_INIT_TRIES; attempt++) {
      g_sensor_ok = MPU6050_Init(&hi2c1);
      if (g_sensor_ok) {
          break;
      }
      char msg[96];
      sprintf(msg, "[I2C] MPU6050 no answer (%u/%u). check VCC/GND/SCL=PB6/SDA=PB7/AD0\r\n",
              (unsigned)attempt, (unsigned)SENSOR_INIT_TRIES);
      say(msg);
      MPU6050_BusRecover(&hi2c1);     /* 掉电重启后从机把 SDA 拉死是常见故障 */
      HAL_Delay(100);
  }

  if (g_sensor_ok) {
      char msg[64];
      sprintf(msg, "[I2C] MPU6050 OK, WHO_AM_I=0x%02X\r\n", (unsigned)MPU6050_GetWhoAmI());
      say(msg);
  } else {
      /* 没找到传感器：命令行仍然可用，LED 常亮当"故障灯" */
      say("[I2C] MPU6050 NOT FOUND. CLI stays alive; s/r/d only report the error.\r\n");
      say("[I2C] wiring: VCC=3.3V, GND=GND, SCL=PB6, SDA=PB7, AD0->GND (addr 0x68)\r\n");
  }

  /* 采集器/AI 库初始化：无论传感器在不在都要跑，保证串口命令有问必答 */
  Collector_Init(&htim2, &huart1, &hi2c1, g_sensor_ok);
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

  /* 传感器缺失 -> LED 常亮（低电平点亮），拔掉串口也能一眼看出问题 */
  if (!g_sensor_ok) {
      led_write(GPIO_PIN_RESET);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  Collector_Process();   /* 采样/录制/输出状态机 */
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  *
  * @note v4：HSE（Blue Pill 板载 8MHz 晶振）起不来时，自动退回内部 HSI。
  *       v3 遇到这种情况会进 Error_Handler 死循环，串口一行字都没有 —— 现场
  *       很容易误判成"固件没跑/串口坏了"。注意：CubeMX 重新生成代码会覆盖
  *       这个函数，改 .ioc 重新生成后要按本函数重新加一次 HSI 回退。
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    /* ---- HSE 失败：退回 HSI/2 * 16 = 64MHz，至少让串口能说话 ---- */
    RCC_OscInitTypeDef RCC_OscHsi = {0};
    RCC_OscHsi.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscHsi.HSIState = RCC_HSI_ON;
    RCC_OscHsi.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscHsi.PLL.PLLState = RCC_PLL_ON;
    RCC_OscHsi.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscHsi.PLL.PLLMUL = RCC_PLL_MUL16;
    if (HAL_RCC_OscConfig(&RCC_OscHsi) != HAL_OK)
    {
      Error_Handler();      /* 连 HSI 都起不来：只能靠 LED 快闪报错 */
    }
    g_clk_hsi = 1;
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

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

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  *
  * @note v4：不再是"静默死循环"。PC13 以约 5Hz 快闪，表示"MCU 还活着，
  *       但某个外设初始化失败"，用肉眼就能和"完全没电/没烧进去"区分开。
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  gpio.Pin   = GPIO_PIN_13;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &gpio);

  __enable_irq();          /* HAL_Delay 依赖 SysTick 中断 */

  while (1)
  {
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(100);        /* 5Hz 快闪 = 启动失败 */
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
