
#include "main.h"
#include <stdlib.h>

/* Peripheral handles */
ADC_HandleTypeDef hadc1;

/* Function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
void Error_Handler(void);

/* LED pins: PA8 - PA12 */
static const uint16_t ledPins[5] = {
    GPIO_PIN_8,
    GPIO_PIN_9,
    GPIO_PIN_10,
    GPIO_PIN_11,
    GPIO_PIN_12
};

/* Button pins: PB5 - PB9 */
static const uint16_t buttonPins[5] = {
    GPIO_PIN_5,
    GPIO_PIN_6,
    GPIO_PIN_7,
    GPIO_PIN_8,
    GPIO_PIN_9
};

/* Turn off all LEDs */
static void LEDs_Off(void)
{
    HAL_GPIO_WritePin(GPIOA,
        GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
        GPIO_PIN_11 | GPIO_PIN_12,
        GPIO_PIN_RESET);
}

/* Turn on one LED */
static void LED_On(uint8_t index)
{
    LEDs_Off();

    if (index < 5U) {
        HAL_GPIO_WritePin(GPIOA, ledPins[index], GPIO_PIN_SET);
    }
}

/* Read the potentiometer on PA0 / ADC1_IN0 */
static uint32_t Read_Potentiometer(void)
{
    uint32_t value = 0U;

    if (HAL_ADC_Start(&hadc1) == HAL_OK) {
        if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK) {
            value = HAL_ADC_GetValue(&hadc1);
        }

        HAL_ADC_Stop(&hadc1);
    }

    return value;
}

/* Map ADC value 0-4095 to reaction time 500-5000 ms */
static uint32_t Get_Reaction_Time(void)
{
    uint32_t adcValue = Read_Potentiometer();

    return 500U + ((adcValue * 4500U) / 4095U);
}

/* Return button index 0-4, or -1 if none is pressed */
static int8_t Read_Button(void)
{
    uint8_t i;

    for (i = 0U; i < 5U; i++) {
        if (HAL_GPIO_ReadPin(GPIOB, buttonPins[i]) == GPIO_PIN_RESET) {
            HAL_Delay(15U); /* Basic debounce */

            if (HAL_GPIO_ReadPin(GPIOB, buttonPins[i]) == GPIO_PIN_RESET) {
                return (int8_t)i;
            }
        }
    }

    return -1;
}

/* Wait until every button is released */
static void Wait_Button_Release(void)
{
    while (Read_Button() != -1) {
        HAL_Delay(10U);
    }

    HAL_Delay(30U);
}

/* Flash all LEDs when the game is over */
static void Game_Over(void)
{
    uint8_t i;

    for (i = 0U; i < 5U; i++) {
        HAL_GPIO_WritePin(GPIOA,
            GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
            GPIO_PIN_11 | GPIO_PIN_12,
            GPIO_PIN_SET);

        HAL_Delay(300U);
        LEDs_Off();
        HAL_Delay(300U);
    }
}

int main(void)
{
    uint8_t failedAttempts = 0U;
    uint32_t reactionTime;
    uint32_t startTime;
    uint8_t targetLED;
    uint8_t success;
    int8_t pressedButton;

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_ADC1_Init();

    LEDs_Off();

    /* Seed random number generator */
    srand(HAL_GetTick() + Read_Potentiometer());

    while (1) {
        /* Pick a random LED */
        targetLED = (uint8_t)(rand() % 5);

        /* Read the current reaction-time setting */
        reactionTime = Get_Reaction_Time();

        /* Show the target */
        LED_On(targetLED);

        startTime = HAL_GetTick();
        success = 0U;

        /* Wait for a button press until timeout */
        while ((HAL_GetTick() - startTime) < reactionTime) {
            pressedButton = Read_Button();

            if (pressedButton != -1) {
                if ((uint8_t)pressedButton == targetLED) {
                    success = 1U;
                    LEDs_Off();
                    Wait_Button_Release();
                    break;
                } else {
                    /* Wrong button: keep waiting */
                    Wait_Button_Release();
                }
            }
        }

        LEDs_Off();

        if (success != 0U) {
            /* Correct answer resets consecutive failures */
            failedAttempts = 0U;
            HAL_Delay(200U);
        } else {
            /* Timeout counts as one failure */
            failedAttempts++;
            HAL_Delay(300U);

            if (failedAttempts >= 3U) {
                Game_Over();
                failedAttempts = 0U;
                Wait_Button_Release();
                HAL_Delay(500U);
            }
        }
    }
}

/* System clock: HSE 25 MHz -> SYSCLK 84 MHz
   IMPORTANT: This assumes your board supplies a 25 MHz HSE clock. */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 168;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

/* ADC1 channel 0: PA0 */
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;

    if (HAL_ADC_Init(&hadc1) != HAL_OK) {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
}

/* GPIO initialization */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Set LEDs LOW before configuring output mode */
    HAL_GPIO_WritePin(GPIOA,
        GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
        GPIO_PIN_11 | GPIO_PIN_12,
        GPIO_PIN_RESET);

    /* PA8-PA12: LED outputs */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 |
                          GPIO_PIN_10 | GPIO_PIN_11 |
                          GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB5-PB9: buttons with internal pull-up */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6 |
                          GPIO_PIN_7 | GPIO_PIN_8 |
                          GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PA0: analog input for potentiometer */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();

    while (1) {
        /* Stop here if initialization fails */
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
