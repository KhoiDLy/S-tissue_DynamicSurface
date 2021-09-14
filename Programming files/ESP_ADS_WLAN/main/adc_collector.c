/*
adc_collector.c will handle the following function

+ ADS1256_cmd() to send command to the ADS1256
+ ADS1256_read() to collect binary data from 6 ADC channels
+ ADS1256_REG() to write to ADS1256 registers
+ ADS1256_init() to initialize the ADS1256 ( called from data_collection_main.c )
+ timer_isr_adc() reset the timer interrupt group 0 timer 0 and setup the xSemaphoreGiveFromISR
+ init_timer() to initialize the timer interrupt ( called from data_collection_main.c )
+ init_buffer() to initialize the buffer that will be sent to PC client via TCP/IP ( called from data_collection_main.c )
+ ADS1256_collect() performs the following actions:
	1. Collect data from 6 ADC channels using MUX as soon as it gets triggered by the ISR
	2. Check if the ADS1256_collect() has happened 5 times. If yes, collect the data from the Queue sent by the receive_data task
	3. Perform local controller functions
	4. Check if the buffer is read to send to the PC client. If yes, flag it.
*/

#include <stddef.h>
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "driver/timer.h" 
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include <stdlib.h>
#include <string.h>
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "rom/ets_sys.h"
#include <time.h>
#include "sys/queue.h"

#include "esp_adc_cal.h"
#include "driver/gpio.h"
#include "adc_collector.h"
#include "esp_log.h"
#include "freertos/semphr.h"

QueueHandle_t xQueueToPC = 0;

uint32_t adc_val[6] = {0,0,0,0,0,0}; // store readings in array


void ADS1256_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

uint8_t ADS1256_read(spi_device_handle_t spi)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;
    t.rxlength=8;                 //Len is in bytes, transaction length is in bits.
    t.flags = SPI_TRANS_USE_RXDATA;

    ret = spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.

    return *(uint8_t*)t.rx_data;
}

void ADS1256_REG(spi_device_handle_t spi, const uint8_t reg, const uint8_t cmd, const uint8_t data) {
	esp_err_t ret;
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length=24;
	t.flags=SPI_TRANS_USE_TXDATA;
	t.tx_data[0] = (0x50 | reg); // REGISTER
	t.tx_data[1] = cmd;          // 2nd command byte, write one register only
	t.tx_data[2] = data;         // Sending data to toggle register
	ret=spi_device_polling_transmit(spi, &t);  //Transmit!
	assert(ret==ESP_OK);            //Should have had no issues.
}

void ADS1256_init(spi_device_handle_t spi) {

    	uint8_t reg = 0, cmd = 0, data = 0;

    	//Initialize non-SPI GPIOs
	gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_DRDY, GPIO_MODE_INPUT);
	gpio_set_direction(PIN_NUM_LED, GPIO_MODE_OUTPUT);

	// Set PULL UP for DRDY Pin
	gpio_set_pull_mode(PIN_NUM_DRDY, GPIO_FLOATING);

	// Hold reset level HIGH
	gpio_set_level(PIN_NUM_RST, 1);
	
	vTaskDelay(1000 / portTICK_PERIOD_MS);

	cmd = 0xFE; //Reset to Power-Up Values 0b11111110
	ADS1256_cmd(spi, cmd);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	reg = (0x50 | 0x00); cmd = 0x00; data = 0x01; // 01h => status: MSB First, Auto-Calibration Disabled, Analog Input Buffer Disabled
	ADS1256_cmd(spi, reg); // Setup Status Register
	ADS1256_cmd(spi, cmd);
	ADS1256_cmd(spi, data);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	reg = (0x50 | 0x02); cmd = 0x00; data = 0x01; // 01h => Clock Out = Off, Sensor Detect OFF, gain 2
	// 0x02 = 0b10 // 0x00 = 0b00 // 0x01 = 0b01
	ADS1256_cmd(spi, reg); // Setup A/D Control Register
	ADS1256_cmd(spi, cmd);
	ADS1256_cmd(spi, data);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	reg = (0x50 | 0x03); cmd = 0x00; data = 0xF0;  // F0h = 11110000 = 30,000SPS
	// 0x02 = 0b11 // 0x00 = 0b00 // 0x01 = 0b01
	ADS1256_cmd(spi, reg); // Setup A/D Data Rate
	ADS1256_cmd(spi, cmd);
	ADS1256_cmd(spi, data);
	vTaskDelay(10 / portTICK_PERIOD_MS);
	cmd = 0xF0; // Perform Offset and Gain Self-Calibration (F0h)
	// 0xF0 = 0b11110000
	ADS1256_cmd(spi, cmd);
}

static void timer_isr_ADS(void* arg)
{

	BaseType_t xHigherPriorityTaskWoken = pdFALSE;	
	
	TIMERG0.int_clr_timers.t0 = 1;
	TIMERG0.hw_timer[0].config.alarm_en = 1;
	
	// code which runs in the interrupt	

	xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);

	if ( xHigherPriorityTaskWoken ){	
		portYIELD_FROM_ISR();
	}

}

// This is going to set up a hardware interrupt.  Again, don't mess with this
// unless you want to break things, or you know what you're doing.
void init_timer(int timer_period_us) {
	timer_config_t config = {
		.alarm_en = true,
		.counter_en = false,
		.intr_type = TIMER_INTR_LEVEL,
		.counter_dir = TIMER_COUNT_UP,
		.auto_reload = true,
		.divider = 80			/* 1 us per tic */
	};

	timer_init(TIMER_GROUP_0, TIMER_0, &config);
	timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);
	timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, timer_period_us);
	timer_enable_intr(TIMER_GROUP_0, TIMER_0);
	timer_isr_register(TIMER_GROUP_0, TIMER_0, &timer_isr_ADS, NULL, 0, &s_timer_handle);
		
	timer_start(TIMER_GROUP_0, TIMER_0);
}


void ADS1256_Collect(spi_device_handle_t spi) {
    	
	uint8_t mux[6] = {0x08,0x18,0x28,0x38,0x48,0x58};
	uint8_t i =0; uint8_t reg = 0; uint8_t cmd = 0; uint8_t data = 0; uint8_t k = 1;
	uint16_t GlobalCommand; 

    	xQueueToPC = xQueueCreate(30, sizeof( uint32_t));
    	if( xQueueToPC == NULL )
    	{
     	   printf("xQueueToPC is not created \n");
    	}
    	else { printf("\n xQueueToPC was created successfully \n");}

	
	static bool FLAG1 = 0;
	static bool FLAG2 = 0;
    	gpio_set_direction(5, GPIO_MODE_OUTPUT);
	gpio_set_direction(2, GPIO_MODE_OUTPUT);
	gpio_set_level(5, 0);
	gpio_set_level(2, 0);	

	uint16_t empty_space;
	for (;;) {

		//wait for the notification from the ISR
		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
/*				if(FLAG == 1){*/
					gpio_set_level(2, 0); FLAG2 = 0;			
/*				}*/
/*				else {*/
/*					gpio_set_level(2, 1); FLAG = 1;			*/
/*				}*/
			for (i=0; i<=5; i++) {          // read all 6 single ended channels AIN0, AIN1, AIN2, AIN3, AIN4, AIN5
				while (gpio_get_level(PIN_NUM_DRDY) == 1) {} ; // Wait until the data read pulls LOW
				// Switching between channels
				reg = 0x01;  cmd = 0x00; data = mux[i]; // 1st Command Byte: 0101 0001  0001 = MUX register address 01h

				ADS1256_REG(spi, reg, cmd, data); // Data Byte(s): xxxx 1000  write the databyte to the register(s)

				//SYNC command 1111 1100 (0xFC)
				cmd = 0xFC;
				ADS1256_cmd(spi, cmd);

				//WAKEUP 0000 0000
				cmd = 0x00;
				ADS1256_cmd(spi, cmd);

				// Read Data
				cmd = 0x01;
				ADS1256_cmd(spi, cmd); // Read Data 0000  0001 (01h)       // ********** Step 3 **********

				adc_val[i] = ADS1256_read(spi);
				adc_val[i] <<= 8; //shift to left
				adc_val[i] |= ADS1256_read(spi);
				adc_val[i] <<= 8;
				adc_val[i] |= ADS1256_read(spi);

/* NOTE: i = 1 corresponds to physical channel 0, i =2 corresponds to physical channel 1, etc. */

				if(adc_val[i] > 0x7fffff) {   //if MSB == 1
					adc_val[i] = adc_val[i]-16777216; //do 2's complement
				}

				if ( i == 1) { printf("%zu \n", adc_val[i]);}

				if(((i == 0) | (i == 1)) && (k == 9)){ // Only store the first and the second ADC channels for WIFI sending
					if (!xQueueSend(xQueueToPC, &adc_val[i], 0)) {
		  				printf("Failed to send to xQueueToPC\n");	
	      			}							
				}
			
			}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Local Control Here ///////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

			if (k == 9){
				if (!xQueueReceive(xQueueGlobal, &GlobalCommand, 5)) {
/*					printf("Failed to receive data from xQueueGlobal\n");			*/
				}
				else {
					printf("Global Command is: %u \n", GlobalCommand);
					if(FLAG1 == 1){
						gpio_set_level(5, 0); FLAG1 = 0;			
					}
					else {
						gpio_set_level(5, 1); FLAG1 = 1;			
					}			
				}
			}
			else { }
			k = (k+1)%10;
			
			ets_delay_us(2500);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Storing Data to Buffer ///////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*		    	if(buffer_idx == NUM_CHANN * WINDOW_SIZE / 2) {   */
/*		    		packet = (char*) buffer;*/
/*		    		packet_size = sizeof(uint32_t) * NUM_CHANN * WINDOW_SIZE / 2;*/
/*		    		packet_ready = true;*/
/*		    	}*/

/*		    	if(buffer_idx >= NUM_CHANN * WINDOW_SIZE)	{*/
/*		    		packet = (char*) (buffer + (NUM_CHANN * WINDOW_SIZE / 2));*/
/*		    		packet_size = sizeof(uint32_t) * NUM_CHANN * WINDOW_SIZE / 2;*/
/*		    		packet_ready = true;*/

/*		    		buffer_idx = 0;    	*/
/*		    	}*/
/*			if(FLAG2 == 1){*/
/*				gpio_set_level(2, 0); FLAG2 = 0;			*/
/*			}*/
/*			else {*/
				gpio_set_level(2, 1); FLAG2 = 1;			
/*			}*/
		}
	}
}




