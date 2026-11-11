/* USER CODE BEGIN Header */
/* SealedBoxMonitor FirmWare Version 1 */
/* Mathis Vossaert and David Ulises Trejo Cancino */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ds18b20.h"                            // Temperature sensor
#include <stdio.h>
#include <math.h>
#include "ssd1306.h"							// OLED I2C Screen for debugging
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HIH_I2C_ADDRESS (0x27 << 1)             // I2C address for the HIH sensor
#define HUMIDITY_SAMPLES 10                     // Moving average sample count
#define SCREEN_I2C_ADDRESS (0x78 << 1)

// Temperature alarm thresholds with hysteresis
#define COMP_TEMP_HIGH_THRESH  50.0f            // Trigger high alarm above this
#define COMP_TEMP_HIGH_CLEAR   45.0f            // Clear high alarm below this
#define COMP_TEMP_LOW_THRESH   0.0f             // Trigger low alarm below this
#define COMP_TEMP_LOW_CLEAR    5.0f             // Clear high alarm above this
#define BOX_TEMP_HIGH_THRESH   50.0f            // Trigger high alarm above this
#define BOX_TEMP_HIGH_CLEAR    45.0f            // Clear high alarm below this
#define BOX_TEMP_LOW_THRESH    0.0f             // Trigger low alarm below this
#define BOX_TEMP_LOW_CLEAR     5.0f             // Clear high alarm above this
#define DEW_POINT_ALARM_DIFF   3.0f				// Trigger dew alarm above this
#define DEW_POINT_CLEAR_DIFF   5.0f				// CLear dew alarm below this
#define HUMIDITY_HIGH_THRESH   60.0f			// Trigger high alarm above this
#define HUMIDITY_HIGH_CLEAR    55.0f			// Clear high alarm below this
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
ds18b20_t ds18;                                 // Object that controls the DS18B20

// Debounce, noise protection timers
uint32_t previous_tick_humidity = 0;
uint32_t current_tick_humidity = 0;
const uint32_t interval_humidity = 3600000;     // 1 hour interval (ms)

uint32_t previous_tick_water = 0;
uint32_t current_tick_water = 0;
const uint32_t interval_water = 3000;           // 3 sec interval (ms)

uint32_t previous_tick_door = 0;
uint32_t current_tick_door = 0;
const uint32_t interval_door = 2000; 			// 2 sec interval (ms)

// Sensor data variables
GPIO_PinState door_state;
GPIO_PinState water_state;
float component_temp = 0.0f;
float box_temp = 0.0f;
float box_humidity = 0.0f;
float raw_temp = 0.0f;
float raw_humidity = 0.0f;
float dew_point = 0.0f;

// Humidity data filtering buffer
float humidity_buffer[HUMIDITY_SAMPLES];
uint8_t humidity_buffer_index = 0;
uint8_t humidity_buffer_filled = 0;             // Startup buffer initialization flag

// Alarm states
static uint8_t humidity_alarm = 0;				// High humidity alarm
static uint8_t dew_alarm = 0;					// Condensation danger alarm
static uint8_t water_alarm = 0;					// Water intrusion alarm
static uint8_t door_alarm = 0;					// Door opened alarm
static uint8_t box_temp_alarm = 0;              // Out-of-range alarm for box temp
static uint8_t comp_temp_alarm = 0;             // Out-of-range alarm for component temp
uint16_t status_word = 0;

// Fault flags
uint8_t ds18_fault = 0;
uint8_t hih_fault = 0;

//Screen Display
uint8_t display_initialized = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Process_Analog_Sensors(void);
void Process_Digital_Sensors(void);
void Transmit_Telemetry(void);
void Screen_Display(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Bridge function connecting Timer 1 and the 1-Wire library
void ds18_tim_cb(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        ow_callback(&ds18.ow);
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
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_IWDG_Init();

  /* USER CODE BEGIN 2 */
    // Start Timer 1 in interrupt mode for the 1-Wire bus
    HAL_TIM_Base_Start_IT(&htim1);

    // Initial config for the 1-Wire DS18B20 protocol
    ow_init_t ow_init_struct;
    ow_init_struct.tim_handle = &htim1;           // Link to Timer 1 (1us tick)
    ow_init_struct.gpio = GPIOA;
    ow_init_struct.pin = GPIO_PIN_6;
    ow_init_struct.tim_cb = ds18_tim_cb;          // Link to the callback function above
    ow_init_struct.done_cb = NULL;
    ow_init_struct.rom_id_filter = DS18B20_ID;    // Filter for DS18B20 sensors

    ds18b20_init(&ds18, &ow_init_struct);         // Initialization
    ds18b20_update_rom_id(&ds18);                 // Scan the wire to find the sensor

    // Timeout protection on startup ROM search
    uint32_t init_timeout = HAL_GetTick();
    while(ds18b20_is_busy(&ds18)) {
        if (HAL_GetTick() - init_timeout > 100) { // 100ms timeout
            break;
        }
    }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      Process_Analog_Sensors();
      Process_Digital_Sensors();     //possibility to add
      Transmit_Telemetry();
      Screen_Display();

      // IWDG Refresh
      HAL_IWDG_Refresh(&hiwdg);
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

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Handles analog/digital sensor measurements (DS18B20 & HIH8131)
  */
void Process_Analog_Sensors(void)
{
    uint8_t hih_raw_bytes[4];                   // 4-byte raw buffer for HIH8131

    // 1. Start DS18B20 conversion
    ds18b20_cnv(&ds18);
    uint32_t cnv_timeout = HAL_GetTick();
    while(ds18b20_is_busy(&ds18)) {
        if (HAL_GetTick() - cnv_timeout > 100) {
            break;
        }
    }

    // 2. HIH8131 Measurement Request over I2C
    HAL_StatusTypeDef tx_status = HAL_I2C_Master_Transmit(&hi2c1, HIH_I2C_ADDRESS, NULL, 0, 100);

    // 3. Wait for parallel sensor conversions (~800ms)
    HAL_Delay(800);

    // 4. Collect DS18B20 component temperature with timeouts
    uint32_t cnv_done_timeout = HAL_GetTick();
    while(!ds18b20_is_cnv_done(&ds18)) {
        if (HAL_GetTick() - cnv_done_timeout > 100) {
            break;
        }
    }

    ds18b20_req_read(&ds18, 0);

    uint32_t read_busy_timeout = HAL_GetTick();
    while(ds18b20_is_busy(&ds18)) {
        if (HAL_GetTick() - read_busy_timeout > 100) {
            break;
        }
    }
    //Fault if data is stuck with sensor's initialized value
    float temp_read = ds18b20_read_c(&ds18);
    if (temp_read != 85.0f && temp_read != -127.0f) {
        component_temp = temp_read;
        ds18_fault = 0;

        // Component Temperature Alarm with Hysteresis
        if (component_temp >= COMP_TEMP_HIGH_THRESH || component_temp <= COMP_TEMP_LOW_THRESH) {
            comp_temp_alarm = 1;
        } else if (component_temp < COMP_TEMP_HIGH_CLEAR && component_temp > COMP_TEMP_LOW_CLEAR) {
            comp_temp_alarm = 0;
        }
    }
    else {
        ds18_fault = 1;
    }

    // 5. Collect HIH8131 bytes over I2C
    HAL_StatusTypeDef rx_status = HAL_I2C_Master_Receive(&hi2c1, HIH_I2C_ADDRESS, hih_raw_bytes, 4, 100);

    // 6. Process HIH data if transfers succeeded and status bits are valid (0x00)
    if (tx_status == HAL_OK && rx_status == HAL_OK) {
        uint8_t hih_status = (hih_raw_bytes[0] >> 6) & 0x03;

        if (hih_status == 0x00) {
            hih_fault = 0;

            // Convert 14-bit raw counts to physical values
            uint16_t raw_humidity_counts = (((uint16_t)(hih_raw_bytes[0] & 0x3F)) << 8) | hih_raw_bytes[1];
            raw_humidity = ((float)raw_humidity_counts / 16382.0f) * 100.0f;

            uint16_t raw_temp_counts = (((uint16_t)hih_raw_bytes[2] << 8) | hih_raw_bytes[3]) >> 2;
            raw_temp = ((float)raw_temp_counts / 16382.0f) * 165.0f - 40.0f;
            box_temp = raw_temp;

            // Box Temperature Alarm with Hysteresis
            if (box_temp >= BOX_TEMP_HIGH_THRESH || box_temp <= BOX_TEMP_LOW_THRESH) {
                box_temp_alarm = 1;
            } else if (box_temp < BOX_TEMP_HIGH_CLEAR && box_temp > BOX_TEMP_LOW_CLEAR) {
                box_temp_alarm = 0;
            }

            // Average of 10 values
            if (!humidity_buffer_filled) {
                for (int i = 0; i < HUMIDITY_SAMPLES; i++) { //First 10 values directly used
                    humidity_buffer[i] = raw_humidity;
                }
                humidity_buffer_filled = 1;
            }

            humidity_buffer[humidity_buffer_index] = raw_humidity;
            humidity_buffer_index = (humidity_buffer_index + 1) % HUMIDITY_SAMPLES; //Removes the oldest value for the newest

            float sum = 0.0f;
            for (int j = 0; j < HUMIDITY_SAMPLES; j++) {
                sum += humidity_buffer[j];
            }
            box_humidity = sum / (float)HUMIDITY_SAMPLES;

            // Humidity Alarm Logic (> 60% for 1h)
            if (box_humidity > 60.0f) {
                current_tick_humidity = HAL_GetTick();
                if (current_tick_humidity - previous_tick_humidity >= interval_humidity) {
                    humidity_alarm = 1;
                }
            } else {
                previous_tick_humidity = HAL_GetTick();
            }

            if (box_humidity < 55.0f) {
                humidity_alarm = 0;
            }

            // Dew Point & Condensation Alarm Logic (Magnus-Tetens)
            float safe_humidity = box_humidity;
            if (safe_humidity < 0.1f) {
                safe_humidity = 0.1f;
            } else if (safe_humidity > 100.0f) {         //safe_humidity to exclude undefined domain values for log(x)
                safe_humidity = 100.0f;
            }

            float gamma = (17.27f * box_temp) / (237.7f + box_temp) + logf(safe_humidity / 100.0f);
            dew_point = (237.7f * gamma) / (17.27f - gamma);

            if ((box_temp - dew_point) <= 3.0f) {
                dew_alarm = 1;
            } else if ((box_temp - dew_point) > 5.0f) {
                dew_alarm = 0;
            }
        }
    }
    else {
        hih_fault = 1;
    }
}

/**
  * @brief  Reads and debounces GPIO digital sensors (Water & Door)
  */
void Process_Digital_Sensors(void)
{
    // Water Sensor
    water_state = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7);
    if (water_state == GPIO_PIN_SET) {
        current_tick_water = HAL_GetTick();
        if (current_tick_water - previous_tick_water >= interval_water) {
            water_alarm = 1;
        }
    }
    else {
        previous_tick_water = HAL_GetTick();
    }

    // Door Sensor
    door_state = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0);
    if (door_state == GPIO_PIN_SET){
        current_tick_door = HAL_GetTick();
        if(current_tick_door - previous_tick_door >= interval_door){
            door_alarm = 1;
        }
    }
    else {
        previous_tick_door = HAL_GetTick();
    }
}

/**
  * @brief  Builds status word bitmask, formats telemetry string and transmits via UART
  */
void Transmit_Telemetry(void)
{
    // Construct status word (bitmask) including all faults and alarms of the system
    status_word = 0;
    if (humidity_alarm)    status_word |= (1 << 0); // Bit 0
    if (dew_alarm)         status_word |= (1 << 1); // Bit 1
    if (water_alarm)       status_word |= (1 << 2); // Bit 2
    if (door_alarm)        status_word |= (1 << 3); // Bit 3
    if (box_temp_alarm)    status_word |= (1 << 4); // Bit 4
    if (comp_temp_alarm)   status_word |= (1 << 5); // Bit 5
    if (ds18_fault)        status_word |= (1 << 6); // Bit 6
    if (hih_fault)         status_word |= (1 << 7); // Bit 7

    char buffer[120];
    int length = snprintf(buffer, sizeof(buffer),
                          "CompTemp: %.1fC | BoxTemp: %.1fC | BoxHum: %.1f%% | DewPoint: %.1fC | Door: %d | Status: 0x%02X\r\n",
                          component_temp, box_temp, box_humidity, dew_point, (int)door_state, status_word);

    if (length > 0) {
        if ((size_t)length >= sizeof(buffer)) {
            length = sizeof(buffer) - 1;
        }
        HAL_UART_Transmit(&huart2, (uint8_t*)buffer, (uint16_t)length, 1000);
        door_alarm = 0;  // Reset door latch alarm after transmission, to prevent missing a quick door opening
    }
}
/**
  * @brief  Reads and debounces GPIO digital sensors (Water & Door)
  */
void Screen_Display(void)
{
	//Verify screen is connected
	if (HAL_I2C_IsDeviceReady(&hi2c1, SCREEN_I2C_ADDRESS, 1, 50) == HAL_OK) {

		        //Verify if screen has been initialized
		        if (display_initialized == 0) {
		        	ssd1306_Init();
		            display_initialized = 1;
		        }
		                // Convert data
		                char buf1[32];
		                char buf2[32];
		                char buf3[32];
		                snprintf(buf1, sizeof(buf1), "Temp: %.1f C", box_temp);
		                snprintf(buf2, sizeof(buf2), "Hum: %.1f %%", box_humidity);
		                snprintf(buf3, sizeof(buf3), "Status: 0x%02X", status_word);

		                // Refresh display
		                ssd1306_Fill(Black);

		                // Writing process
		                ssd1306_SetCursor(2, 10);
		                ssd1306_WriteString(buf1, Font_11x18, White);
		                ssd1306_SetCursor(2, 35);
		                ssd1306_WriteString(buf2, Font_11x18, White);
		                ssd1306_SetCursor(2, 60);
		                ssd1306_WriteString(buf3, Font_11x18, White);

		                // Execute changes
		                ssd1306_UpdateScreen();

	}
		        else
		            {
		                // No screen detected, force intialization to 0 for next cycle
		                display_initialized = 0;
		            }


}

/* USER CODE END USER_CODE_4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
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
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
