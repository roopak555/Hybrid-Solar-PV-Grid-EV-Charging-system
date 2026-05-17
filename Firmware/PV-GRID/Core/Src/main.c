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
#include "stdio.h"
#include "string.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */


uint16_t adc_buffer[5];
char uart_msg[100];
uint8_t source_scheduling_mode = 0;

// Variable to store our automatically calibrated zero-point


//DC TO DC
float v_in = 0.0;
float v_out = 0.0;
float display_amps = 0.0;
int pwm_duty_cycle = 0;

float ac_v_out = 0.0;          // NEW: AC Side Output Voltage
float ac_display_amps = 0.0;   // NEW: AC Side Current
int ac_pwm_duty_cycle = 0;

int battery_is_full = 0;
char mode_str[10] = "";
char ac_mode_str[10] = "";

float zero_amp_voltage = 2.5;
float smoothed_current = 0.0;

float ac_zero_amp_voltage = 2.5;
int mapped_pwm = 0;
int system_calibrated = 0;

uint8_t rx_byte;
char rx_buffer[20];
int rx_index = 0;
uint8_t pi_charging_active = 0;
uint8_t auto_mode = 1; // NEW: The Brains of the Operation


float cv_target_voltage = 14.40;
float ovp_limit = 14.60;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
void Read_And_Filter_Sensors(void);
void Run_Charge_Controller(void);
void Print_System_Status(void);
void Run_AC_DC_Controller(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {

        if (rx_byte == 'D') {
            pi_charging_active = 0;
            source_scheduling_mode = 0;
            auto_mode = 0; // Turn off auto mode
            pwm_duty_cycle = 0;
            ac_pwm_duty_cycle = 0;
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
            strcpy(mode_str, "DISCONN");
            strcpy(ac_mode_str, "DISCONN");
        }

        if (rx_byte == '\n' || rx_byte == '\r') {
            rx_buffer[rx_index] = '\0';

            if (strstr(rx_buffer, "START") != NULL) {
                pi_charging_active = 1;
            }
            else if (strstr(rx_buffer, "LEAD") != NULL) {
                cv_target_voltage = 14.40;
                ovp_limit = 14.60;
            }
            else if (strstr(rx_buffer, "LION") != NULL) {
                cv_target_voltage = 12.60;
                ovp_limit = 12.80;
            }
            else if (strstr(rx_buffer, "SCHED_ON") != NULL) {
                source_scheduling_mode = 1;
                auto_mode = 0; // Disable auto if forced ON
                pi_charging_active = 1;
            }
            else if (strstr(rx_buffer, "SCHED_AUTO") != NULL) {
                auto_mode = 1; // Enable the Smart Logic
                pi_charging_active = 1;
            }
            else if (strstr(rx_buffer, "SCHED_OFF") != NULL) {
                source_scheduling_mode = 0;
                auto_mode = 0;
            }
            else if (strstr(rx_buffer, "STOP") != NULL || strstr(rx_buffer, "DISCONNECT") != NULL) {
                pi_charging_active = 0;
                source_scheduling_mode = 0;
                auto_mode = 0;
                pwm_duty_cycle = 0;
                ac_pwm_duty_cycle = 0;
                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
                __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);
                strcpy(mode_str, "DISCONN");
                strcpy(ac_mode_str, "DISCONN");
            }

            memset(rx_buffer, 0, sizeof(rx_buffer));
            rx_index = 0;
        } else {
            if (rx_index < 19) rx_buffer[rx_index++] = rx_byte;
        }

        HAL_UART_Receive_IT(&huart3, &rx_byte, 1);
    }
}
// ==========================================================
// 3. THE UART AUTO-RECOVERY LISTENER
// If the 100ms sensor delay causes an "Overrun Error", this
// instantly clears the crash so the STM32 doesn't go permanently deaf.
// ==========================================================
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        HAL_UART_AbortReceive_IT(huart);
        HAL_UART_Receive_IT(&huart3, &rx_byte, 1); // Force it to listen again
    }
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  // Start listening to the Raspberry Pi on UART3
  HAL_UART_Receive_IT(&huart3, &rx_byte, 1);

    // 1. Start the hardware
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, 5); // Waits for TIM3
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);            // DC PWM (PA8)
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);            // AC PWM (PA9)
    HAL_TIM_Base_Start(&htim3);                          // 10kHz Metronome

    HAL_Delay(500); // Let the sensors stabilize

    // 2. RUN THE CALIBRATION
    char msg1[] = "\r\n--- STARTING 30A DC & AC CALIBRATION ---\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t*)msg1, strlen(msg1), 100);

    uint32_t dc_cal_sum = 0; uint16_t dc_min_raw = 4095; uint16_t dc_max_raw = 0;
    uint32_t ac_cal_sum = 0; uint16_t ac_min_raw = 4095; uint16_t ac_max_raw = 0;

    for (int i = 0; i < 100; i++) {
        uint16_t dc_current_raw = adc_buffer[0];
        dc_cal_sum += dc_current_raw;
        if (dc_current_raw < dc_min_raw) dc_min_raw = dc_current_raw;
        if (dc_current_raw > dc_max_raw) dc_max_raw = dc_current_raw;

        uint16_t ac_current_raw = adc_buffer[3];
        ac_cal_sum += ac_current_raw;
        if (ac_current_raw < ac_min_raw) ac_min_raw = ac_current_raw;
        if (ac_current_raw > ac_max_raw) ac_max_raw = ac_current_raw;

        HAL_Delay(10);
    }

    float dc_cal_avg_raw = (float)dc_cal_sum / 100.0;
    zero_amp_voltage = (dc_cal_avg_raw / 4095.0) * 3.3;

    float ac_cal_avg_raw = (float)ac_cal_sum / 100.0;
    ac_zero_amp_voltage = (ac_cal_avg_raw / 4095.0) * 3.3;

    sprintf(uart_msg, "DC LOCKED ZERO: %.3f V\r\n", zero_amp_voltage);
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_msg, strlen(uart_msg), 100);
    sprintf(uart_msg, "AC LOCKED ZERO: %.3f V\r\n\n", ac_zero_amp_voltage);
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_msg, strlen(uart_msg), 100);

    HAL_Delay(1000);

        // FLIP THE SWITCH: Start ramping the PWM now!
    system_calibrated = 1;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    uint32_t last_print_time = 0;
    while (1)
  {
    /* USER CODE END WHILE */
	  if (HAL_GetTick() - last_print_time >= 200) {
	            Print_System_Status();
	            last_print_time = HAL_GetTick();
	        }

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

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
    Error_Handler();
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 5;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 23;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 99;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 71;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Read_And_Filter_Sensors(void)
{
    // 1. Grab instantly from the DMA
    float temp_v_in  = ((float)adc_buffer[1] / 4095.0) * 35.79;
    float temp_v_out = ((float)adc_buffer[2] / 4095.0) * 35.78;
    float temp_ac_v  = ((float)adc_buffer[4] / 4095.0) * 35.23;

    // Convert DC Amps
    float dc_calibration = 1.45;
        float raw_amps = (((((float)adc_buffer[0] / 4095.0) * 3.3) - zero_amp_voltage) / 0.066) * dc_calibration;
        float temp_current_amps = raw_amps - 0.12;
        if (temp_current_amps > -0.05 && temp_current_amps < 0.05) temp_current_amps = 0.00;

    // Convert AC Amps
        float ac_calibration = 0.79;

            float ac_raw_amps = ((((float)adc_buffer[3] / 4095.0) * 3.3) - ac_zero_amp_voltage) / 0.100;
            float temp_ac_amps = ((ac_raw_amps * 2.452) - 0.12) * ac_calibration;

            if (temp_ac_amps > -0.05 && temp_ac_amps < 0.05) temp_ac_amps = 0.00;

    // ========================================================
    // THE FIX: Floor the TEMP variables so the math never traps!
    // ========================================================
    if (temp_v_in < 0.3) temp_v_in = 0.00;
    if (temp_v_out < 0.3) temp_v_out = 0.00;
    if (temp_ac_v < 0.3) temp_ac_v = 0.00;

    if (temp_current_amps > -0.05 && temp_current_amps < 0.05) temp_current_amps = 0.00;
    if (temp_ac_amps > -0.05 && temp_ac_amps < 0.05) temp_ac_amps = 0.00;

    // 2. 1% EMA FILTER
    if (v_out == 0.0 && temp_v_out > 0.0) {
        // Instant initialization on boot
        v_in = temp_v_in;
        v_out = temp_v_out;
        ac_v_out = temp_ac_v;
        display_amps = temp_current_amps;
        ac_display_amps = temp_ac_amps;
    } else {
        // Smooth mathematical blending
        v_in = (temp_v_in * 0.01) + (v_in * 0.99);
        v_out = (temp_v_out * 0.01) + (v_out * 0.99);
        ac_v_out = (temp_ac_v * 0.01) + (ac_v_out * 0.99);

        display_amps = (temp_current_amps * 0.002) + (display_amps * 0.998);
        ac_display_amps = (temp_ac_amps * 0.005) + (ac_display_amps * 0.995);    }
}
void Run_Charge_Controller(void)
{
    // --- THE AUTO SMART LOGIC (FIXED HYSTERESIS & NIGHT MODE) ---
    if (auto_mode == 1) {
        if (v_in < 13.0) {
            // NIGHT: Solar is dead. Grid takes 100% load.
            source_scheduling_mode = 0;
        }
        else if (v_in >= 13.0 && v_in < 16.0) {
            // CLOUD: Sun is weak. Turn Hybrid ON (Split 0.5A / 0.5A)
            source_scheduling_mode = 1;
        }
        else if (v_in >= 16.0) {
            // STRONG SUN: Turn Hybrid OFF (Solar takes 100% load)
            source_scheduling_mode = 0;
        }
    }
    // ------------------------------------------------------------

    float max_charge_current = (source_scheduling_mode == 1) ? 0.50 : 1.00;
    float tail_current_limit = 0.60;
    int pwm_step = 1;

    static uint16_t dc_timer = 0;
    static uint16_t cv_full_counter = 0;
    static uint8_t in_cv_mode = 0;

    dc_timer++;

    if (v_out < (cv_target_voltage - 1.0)) {
        battery_is_full = 0;
        in_cv_mode = 0;
    }

    // INSTANT SAFETY OFF: If panel collapses below 8.0V, instantly kill DC PWM.
    if (v_in < 13.0 || battery_is_full || v_out >= ovp_limit || pi_charging_active == 0) {
        pwm_duty_cycle = 0;
        if(battery_is_full) {
            strcpy(mode_str, "FULL");
            source_scheduling_mode = 0;
            auto_mode = 0;
        }
        else strcpy(mode_str, "OFF/WAIT");
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
        return;
    }

    if (dc_timer >= 200) {
        dc_timer = 0;
        if (v_out >= cv_target_voltage) in_cv_mode = 1;

        if (in_cv_mode) {
            if (v_out > (cv_target_voltage + 0.02)) {
                if (pwm_duty_cycle > 0) pwm_duty_cycle -= pwm_step;
                strcpy(mode_str, "CV-DN");
            }
            else if (v_out < (cv_target_voltage - 0.02)) {
                if (display_amps < (max_charge_current + 0.1)) {
                    if (pwm_duty_cycle < 90) pwm_duty_cycle += pwm_step;
                }
                strcpy(mode_str, "CV-UP");
            }
            else { strcpy(mode_str, "CV-HLD"); }

            if (display_amps < tail_current_limit && v_out >= (cv_target_voltage - 0.05)) {
                cv_full_counter++;
                if (cv_full_counter > 30) {
                    battery_is_full = 1;
                    source_scheduling_mode = 0;
                    auto_mode = 0;
                }
            } else { cv_full_counter = 0; }
        }
        else {
            if (display_amps > (max_charge_current + 0.05)) {
                pwm_duty_cycle -= pwm_step;
                strcpy(mode_str, "CC-DN");
            }
            else if (display_amps < (max_charge_current - 0.05)) {
                if (pwm_duty_cycle < 90) pwm_duty_cycle += pwm_step;
                strcpy(mode_str, "CC-UP");
            }
            else { strcpy(mode_str, "CC-HLD"); }
        }
    }

    if (pwm_duty_cycle > 90) pwm_duty_cycle = 90;
    if (pwm_duty_cycle < 0) pwm_duty_cycle = 0;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pwm_duty_cycle);
}
void Run_AC_DC_Controller(void)
{
    int max_ac_pwm = 18;
    int pwm_step = 1;
    float ac_tail_current = 0.60;

    float ac_cc_max = (source_scheduling_mode == 1) ? 0.60 : 0.60;
    float ac_cc_min = (source_scheduling_mode == 1) ? 0.40 : 0.40;

    static uint16_t ac_timer = 0;
    static uint16_t ac_cv_full_counter = 0;
    ac_timer++;

    if (v_out < 3.0 && ac_v_out < 3.0){
        ac_pwm_duty_cycle = 0;
        strcpy(ac_mode_str, "NO-BATT");
    }
    else if (v_in >=13.0 && source_scheduling_mode == 0) {
        ac_pwm_duty_cycle = 0;
        strcpy(ac_mode_str, "STBY");
    }
    else if (battery_is_full == 1) {
        ac_pwm_duty_cycle = 0;
        strcpy(ac_mode_str, "FULL");
        source_scheduling_mode = 0;
    }
    else if (ac_v_out >= ovp_limit) {
        ac_pwm_duty_cycle = 0;
        strcpy(ac_mode_str, "OVP");
    }
    else if (ac_display_amps > 1.50) {
        ac_pwm_duty_cycle = 0;
        strcpy(ac_mode_str, "AC-TRIP");
    }
    else if (pi_charging_active == 0) {
        ac_pwm_duty_cycle = 0;
        strcpy(ac_mode_str, "WAITING");
    }
    else if (ac_timer >= 200) {
        ac_timer = 0;

        if (ac_v_out >= cv_target_voltage) {
            if (ac_v_out > (cv_target_voltage + 0.02)) {
                if (ac_pwm_duty_cycle > 0) ac_pwm_duty_cycle -= pwm_step;
                strcpy(ac_mode_str, "AC-CV-D");
            } else if (ac_v_out < (cv_target_voltage - 0.02)) {
                if (ac_pwm_duty_cycle < max_ac_pwm) ac_pwm_duty_cycle += pwm_step;
                strcpy(ac_mode_str, "AC-CV-U");
            } else { strcpy(ac_mode_str, "AC-CV-H"); }

            if (ac_display_amps <= ac_tail_current && ac_v_out >= (cv_target_voltage - 0.05)) {
                ac_cv_full_counter++;
                if (ac_cv_full_counter > 20) {
                    battery_is_full = 1;
                    strcpy(ac_mode_str, "FULL");
                    source_scheduling_mode = 0;
                }
            } else { ac_cv_full_counter = 0; }
        }
        else {
            ac_cv_full_counter = 0;
            if (ac_display_amps > ac_cc_max) {
                if (ac_pwm_duty_cycle > 0) ac_pwm_duty_cycle -= pwm_step;
                strcpy(ac_mode_str, "AC-CC");
            }
            else if (ac_display_amps < ac_cc_min) {
                if (ac_pwm_duty_cycle < max_ac_pwm) ac_pwm_duty_cycle += pwm_step;
                strcpy(ac_mode_str, "AC-RMP");
            }
            else { strcpy(ac_mode_str, "AC-HLD"); }
        }
    }

    if (ac_pwm_duty_cycle > max_ac_pwm) ac_pwm_duty_cycle = max_ac_pwm;
    if (ac_pwm_duty_cycle < 0) ac_pwm_duty_cycle = 0;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ac_pwm_duty_cycle);
}
void Print_System_Status(void)
{
	sprintf(uart_msg, "DC: %-7s | IN: %5.2fV  | OUT: %5.2fV | %5.2fA | PWM: %d%%\r\n",
	                mode_str, v_in, v_out, display_amps, pwm_duty_cycle);
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_msg, strlen(uart_msg), 100);

    sprintf(uart_msg, "AC: %-7s | --: -----V | OUT: %5.2fV | %5.2fA | PWM: %d%%\r\n\n",
            ac_mode_str, ac_v_out, ac_display_amps, ac_pwm_duty_cycle);
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_msg, strlen(uart_msg), 100);

    char pi_msg[120];
    float current_volt = (v_out > ac_v_out) ? v_out : ac_v_out;

    if (current_volt < 3.0) {
        sprintf(pi_msg, "BATT:0\r\n");
        pi_charging_active = 0;
        source_scheduling_mode = 0;
    } else {
        char src_str[10];
        float active_amps = 0.0;

        if (battery_is_full == 1) {
            strcpy(src_str, "FULL");
            active_amps = 0.0;
        } else if (source_scheduling_mode == 1) {
            strcpy(src_str, "HYBRID");
            active_amps = display_amps + ac_display_amps;
        } else if (v_in >= 13.0) { // Changed to 8.0 to match your standby logic
            strcpy(src_str, "PV");
            active_amps = display_amps;
        } else {
            strcpy(src_str, "GRID");
            active_amps = ac_display_amps;
        }

        // Send the individual DC and AC values to split the text on the GUI
        sprintf(pi_msg, "BATT:1,VOLT:%.2f,AMP:%.2f,DC:%.2f,AC:%.2f,SRC:%s\r\n",
                current_volt, active_amps, display_amps, ac_display_amps, src_str);
    }

    HAL_UART_Transmit(&huart3, (uint8_t*)pi_msg, strlen(pi_msg), 100);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_13);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	if (hadc->Instance == ADC1) {
	        // 1. ALWAYS read and filter the sensors so the EMA stabilizes
	        Read_And_Filter_Sensors();

	        // 2. ONLY ramp the PWM if calibration is completely done
	        if (system_calibrated == 1) {
	            Run_Charge_Controller();
	            Run_AC_DC_Controller();
	        }
	    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
