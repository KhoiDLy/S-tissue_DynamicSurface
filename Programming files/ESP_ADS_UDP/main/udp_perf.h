/* udp_perf Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef __UDP_PERF_H__
#define __UDP_PERF_H__



#ifdef __cplusplus
extern "C" {
#endif


/*test options*/
#define EXAMPLE_ESP_WIFI_MODE_AP CONFIG_UDP_PERF_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_UDP_MODE_SERVER CONFIG_UDP_PERF_SERVER //TRUE:server FALSE:client
#define EXAMPLE_ESP_UDP_PERF_TX CONFIG_UDP_PERF_TX //TRUE:send FALSE:receive
#define EXAMPLE_ESP_UDP_DELAY_INFO CONFIG_UDP_PERF_DELAY_DEBUG //TRUE:show delay time info

/*AP info and udp_server info*/
#define EXAMPLE_DEFAULT_SSID CONFIG_UDP_PERF_WIFI_SSID
#define EXAMPLE_DEFAULT_PWD CONFIG_UDP_PERF_WIFI_PASSWORD
#define EXAMPLE_DEFAULT_PORT CONFIG_UDP_PERF_SERVER_PORT
#define EXAMPLE_DEFAULT_PKTSIZE CONFIG_UDP_PERF_PKT_SIZE
#define EXAMPLE_MAX_STA_CONN 1 //how many sta can be connected(AP mode)

#ifdef CONFIG_UDP_PERF_SERVER_IP
#define EXAMPLE_DEFAULT_SERVER_IP CONFIG_UDP_PERF_SERVER_IP
#else
#define EXAMPLE_DEFAULT_SERVER_IP "192.168.4.1"
#endif /*CONFIG_UDP_PERF_SERVER_IP*/


#define CONFIG_UDP_PERF_ESP_IS_STATION 1
#define CONFIG_UDP_PERF_ESP_IS_SERVER 1
#define CONFIG_UDP_PERF_SERVER 1
#define CONFIG_UDP_PERF_ESP_SEND 1
// #define CONFIG_UDP_PERF_ESP_RECV
#define CONFIG_UDP_PERF_TX 1
// #define CONFIG_UDP_PERF_DELAY_DEBUG
#define CONFIG_UDP_PERF_WIFI_SSID "ESP32 2.4"
#define CONFIG_UDP_PERF_WIFI_PASSWORD "Cao5566thU"
#define CONFIG_UDP_PERF_SERVER_PORT 5111
#define CONFIG_UDP_PERF_PKT_SIZE 4

#define EXAMPLE_PACK_BYTE_IS 97 //'a'
#define TAG "udp_perf:"

/* FreeRTOS event group to signal when we are connected to wifi*/
extern EventGroupHandle_t udp_event_group;
#define WIFI_CONNECTED_BIT BIT0

extern int  g_total_data;
extern bool g_rxtx_need_restart;

extern TaskHandle_t tx_task;
extern TaskHandle_t rx_task; 
extern QueueHandle_t xQueueGlobal;
extern QueueHandle_t xQueueToPC;

#if EXAMPLE_ESP_UDP_PERF_TX && EXAMPLE_ESP_UDP_DELAY_INFO
extern int g_total_pack;
extern int g_send_success;
extern int g_send_fail;
extern int g_delay_classify[5];
#endif/*EXAMPLE_ESP_UDP_PERF_TX && EXAMPLE_ESP_UDP_DELAY_INFO*/


//using esp as station
void wifi_init_sta();
//using esp as softap
void wifi_init_softap();

//create a udp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_server();
//create a udp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_client();

//send data task
void send_data(void *pvParameters);
//receive data task
void recv_data(void *pvParameters);

//close all socket
void close_socket();

//get socket error code. return: error code
int get_socket_error_code(int socket);

//show socket error code. return: error code
int show_socket_error_reason(const char* str, int socket);

//check working socket
int check_working_socket();


#ifdef __cplusplus
}
#endif


#endif /*#ifndef __UDP_PERF_H__*/

