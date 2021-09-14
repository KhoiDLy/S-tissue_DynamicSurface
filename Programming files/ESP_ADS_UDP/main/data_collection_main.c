/* 
data_collection_main. c handles the following functions

1. Setup the SPI protocol
2. Call the ADS1256_init() located in adc_collector.c to setup the ADS1256 Registers, channels, sampling speed, sampling mode, etc
3. Initialize the buffer using init_buffer() located in adc_collector.c
4. Initialize the binary semaphore for the Timer interrupt running at 200Hz located in adc_collector.c
5. Create task for ADS1256_collect() that will be triggered by timer interrupt
6. Initialize the timer interrupt
7. Create task for udp_conn() that will setup WIFI connection, ESP32 UDP server
8. Inside the udp_conn(), create send_data task and receive_data task with receive_data task will be in suspended state 

*/

#include <stdio.h>

#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "esp_spi_flash.h"
#include "sys/queue.h"

#include "udp_perf.h"
#include "adc_collector.h"

SemaphoreHandle_t xSemaphore = NULL;

QueueHandle_t xQueueGlobal = 0;

TaskHandle_t tx_task = NULL;
TaskHandle_t rx_task = NULL;

//this task establish a UDP connection and receive data from UDP
static void udp_conn(void *pvParameters)
{
    while (1) {

        g_rxtx_need_restart = false;

        ESP_LOGI(TAG, "task udp_conn.");
        /*wating for connecting to AP*/
        xEventGroupWaitBits(udp_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "sta has connected to ap.");
        int socket_ret = ESP_FAIL;

#if EXAMPLE_ESP_UDP_MODE_SERVER
        if (socket_ret == ESP_FAIL) {
            /*create udp socket*/
            ESP_LOGI(TAG, "udp_server will start after 3s...");
            vTaskDelay(3000 / portTICK_RATE_MS);
            ESP_LOGI(TAG, "create_udp_server.");
            socket_ret = create_udp_server();
        }
#else /*EXAMPLE_ESP_UDP_MODE_SERVER*/
        if (socket_ret == ESP_FAIL) {
            ESP_LOGI(TAG, "udp_client will start after 20s...");
            vTaskDelay(20000 / portTICK_RATE_MS);
            ESP_LOGI(TAG, "create_udp_client.");
            socket_ret = create_udp_client();
        }
#endif
        if (socket_ret == ESP_FAIL) {
            ESP_LOGI(TAG, "create udp socket error,stop.");
            continue;
        }

        /*create a task to tx/rx data*/

        if (tx_task == NULL) {
            if (pdPASS != xTaskCreatePinnedToCore(&send_data, "send_data", 4096, NULL, 4, &tx_task, 1)) {
                ESP_LOGE(TAG, "Send task create fail!");
            }
        }

	  if (rx_task == NULL) {
            if (pdPASS != xTaskCreatePinnedToCore(&recv_data, "recv_data", 4096, NULL, 5, &rx_task,1)) {
                ESP_LOGE(TAG, "recv task create fail!");
            }
        }

        while (1) {
            g_total_data = 1;
            vTaskDelay(3000 / portTICK_RATE_MS);//every 3s

            if (g_rxtx_need_restart) {
                printf("send or receive task encounter error, need to restart\n");
                break;
            }
        }

        close_socket();
    }

    vTaskDelete(NULL);
}


void app_main(void)
{
    timer_click_count = 0;
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
#if EXAMPLE_ESP_WIFI_MODE_AP
    ESP_LOGI(TAG, "EXAMPLE_ESP_WIFI_MODE_AP");
    wifi_init_softap();
#else
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
#endif /*EXAMPLE_ESP_WIFI_MODE_AP*/

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

    	//Attach the ADS1256 to the SPI busb
    	ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
 	ESP_ERROR_CHECK(ret);

	// Initialize ADS1256 using ADS1256_init() located in adc_collector.c
      ADS1256_init(spi);

    // Once the timer is initialized, the ADCs can be read at any time further, so
    // make sure that the ADCs are set up the way you want, and the buffers are
    // properly allocated.
    xSemaphore = xSemaphoreCreateBinary();

    xQueueGlobal = xQueueCreate(30, sizeof( uint16_t));
    if( xQueueGlobal == NULL )
    {
        printf("Queue is not created \n");
    }
    else { printf("\n Queue was created successfully \n");}

    xTaskCreatePinnedToCore(ADS1256_Collect, "ADS1256_Collect", 10000, (void *) spi, 10, NULL, 0);
   

    init_timer(5000);

    xTaskCreate(&udp_conn, "udp_conn", 4096, NULL, 6, NULL);
}
