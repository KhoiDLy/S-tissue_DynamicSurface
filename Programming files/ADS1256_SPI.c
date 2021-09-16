// Works with all 4 channels, make sure to tie CS LOW properly
// This is the changes in the new-branch
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include <time.h>

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_DRDY 27
#define PIN_NUM_RST  13
#define PIN_NUM_LED  25

void ADS1256_cmd(spi_device_handle_t spi, const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.
}

uint8_t  ADS1256_read(spi_device_handle_t spi)
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

	//	const lcd_init_cmd_t* lcd_init_cmds;

	//Initialize non-SPI GPIOs
	gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_DRDY, GPIO_MODE_INPUT);
	gpio_set_direction(PIN_NUM_LED, GPIO_MODE_OUTPUT);

	// Set PULL UP for DRDY Pin
	gpio_set_pull_mode(PIN_NUM_DRDY, GPIO_FLOATING);

	// Hold reset level HIGH
	gpio_set_level(PIN_NUM_RST, 1);
	gpio_set_level(PIN_NUM_LED, 0);

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

void app_main()
{
	uint8_t reg = 0, cmd = 0, data = 0;
	uint16_t counter = 0;
	uint32_t clock_millis;
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=0,
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=2800*1000,           //Clock out at 10 MHz
        .mode=1,                                //SPI mode 1
        .spics_io_num=-1,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        .pre_cb= NULL,
		.post_cb = NULL,
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 0); // 0 means DMA disabled
    ESP_ERROR_CHECK(ret);
    //Attach the ADS1256 to the SPI bus
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    //Initialize the ADS1256
    ADS1256_init(spi);

	  long adc_val[6] = {0,0,0,0,0,0}; // store readings in array
	  uint8_t mux[6] = {0x08,0x18,0x28,0x38,0x48,0x58};
	  uint8_t i = 0; uint8_t channel = 0;

	vTaskDelay(100 / portTICK_PERIOD_MS);
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("configured, starting\n");
	printf("AIN0	AIN1    AIN2	AIN3    AIN4	AIN5\n");
	while(1){
		  for (i=0; i <= 5; i++){            // read all 6 single ended channels AIN0, AIN1, AIN2, AIN3, AIN4, AIN5
			  channel = mux[i];             // analog in channels #
			  while (gpio_get_level(PIN_NUM_DRDY) == 1) {} ; // Wait until the data read pulls LOW
			  // Switching between channels
			  reg = 0x01;  cmd = 0x00; data = channel; // 1st Command Byte: 0101 0001  0001 = MUX register address 01h
			  ADS1256_REG(spi, reg, cmd, data); // Data Byte(s): xxxx 1000  write the databyte to the register(s)

			  //SYNC command 1111 1100 (0xFC)
			  cmd = 0xFC;
			  ADS1256_cmd(spi, cmd);

			  //while (!digitalRead(rdy)) {} ;
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
			  //ets_delay_us(2);

			  if(adc_val[i] > 0x7fffff){   //if MSB == 1
			  		  adc_val[i] = adc_val[i]-16777216; //do 2's complement
			  }

		  }
/*		  for (i=0; i <= 5; i++){*/
/*			  	  */
/*			  	  printf("%ld",adc_val[i]);   // Raw ADC integer value +/- 23 bits*/
/*			  	  printf("      ");*/
/*		  }*/
/*		  printf("\n");*/
		  vTaskDelay(1 / portTICK_PERIOD_MS);

		  counter = (counter+1)%1000;
		  if (counter == 999) {
		  	clock_millis = (uint32_t) (clock() * 1000 / CLOCKS_PER_SEC);
    			printf("clock_millis: %d\n", clock_millis);
		  }
	}
}
