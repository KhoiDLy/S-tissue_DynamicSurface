/* 
tcp_perf.c handles the following functions

+ event_handler() makes sure the synchronization between tcp_conn and the rest of the functions. Other tasks only run after the connection to PC has been properly established
+ send_data()
	1. Check if buffer flag is set (from adc_collector.c ) and try to write the data to the tcp/ip port
	2. After sending successfully, resume the receive_data() and suspend itself
+ receive_data()
	1. There is no flag, just try to collect data if there is data available in the port
	2. After receiving the complete packet, send the data to the Queue which will be collected by the local controller function in adc_collector.c
	3. Resume the send_data() and suspend itself

+ Other default functionalities for setting up the tcp/ip network and handling socket errors.

*/

#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "sys/queue.h"

#include "tcp_perf.h"
#include "adc_collector.h"

/* FreeRTOS event group to signal when we are connected to wifi */
EventGroupHandle_t tcp_event_group;

/*socket*/
static int server_socket = 0;
static struct sockaddr_in server_addr;
static struct sockaddr_in client_addr;
static unsigned int socklen = sizeof(client_addr);
static int connect_socket = 0;
bool g_rxtx_need_restart = false;

int g_total_data = 0;

int packet_size = sizeof(uint32_t) * NUM_CHANN * WINDOW_SIZE / 2;

#if EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO

int g_total_pack = 0;
int g_send_success = 0;
int g_send_fail = 0;
int g_delay_classify[5] = { 0 };


#endif /*EXAMPLE_ESP_TCP_PERF_TX && EXAMPLE_ESP_TCP_DELAY_INFO*/

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_CONNECTED:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s\n",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join,AID=%d\n",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        xEventGroupSetBits(tcp_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave,AID=%d\n",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        xEventGroupClearBits(tcp_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

//send data
void send_data(void *pvParameters)
{
	
    TickType_t xTicksToWait = 1000 / portTICK_PERIOD_MS;

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 1;

    char* packet;
    int len = 0;
    uint32_t DataToPC[NUM_CHANN]; uint32_t DataToPC_pack;
    vTaskDelay(100 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "start sending...");

  // Initialise the xLastWakeTime variable with the current time.
    xLastWakeTime = xTaskGetTickCount();

    while (1) {

        // Check if ready to send the terrain class
/*        if(packet_ready)*/
/*        {*/
/*            ESP_LOGI(TAG, "Sending Packet...");*/

/*            packet_ready = false;*/
            
            // How many bytes are left to send?
            int to_write = packet_size;

		if (xQueueReceive(xQueueToPC, &DataToPC[0], 0)) {
			if (!xQueueReceive(xQueueToPC, &DataToPC[1], 0)) {
				printf("Receing data from xQueueToPC failed \n");
			}
			else {
				packet = (char*) DataToPC;
/*				printf("DataToPC is %p\n", DataToPC);	*/
		      	//send function
				while (to_write > 0) {
				    len = send(connect_socket, packet + (packet_size - to_write), to_write, 0);

				    if (len > 0) {
				        to_write -= len;
				    } else {
				        int err = get_socket_error_code(connect_socket);

				        if (err != ENOMEM) {
				            show_socket_error_reason("send_data", connect_socket);
				            g_rxtx_need_restart = true;
				            break;
				        }
				    }  
				}
			}
		}					
		else {
			vTaskDelayUntil(&xLastWakeTime, xFrequency);
		}
/*            ESP_LOGI(TAG, "Buffer Sent!");    */
/*		vTaskResume(rx_task);*/
/*		vTaskSuspend(tx_task);*/
/*        }*/

          if(g_rxtx_need_restart)
          {
              break;
          }
    }
    vTaskDelete(NULL);
}

//receive data
void recv_data(void *pvParameters)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = 0;

    ESP_LOGI(TAG, "here in recv");
    int len = 0;
 
    uint32_t *databuff = (uint32_t *)malloc(EXAMPLE_DEFAULT_PKTSIZE * sizeof(char));

    xLastWakeTime = xTaskGetTickCount();
    while (1) {
/*        vTaskDelayUntil(&xLastWakeTime, xFrequency);*/
        int to_recv = EXAMPLE_DEFAULT_PKTSIZE;
	  
        while (to_recv > 0) {
            len = recv(connect_socket, databuff + (EXAMPLE_DEFAULT_PKTSIZE - to_recv), to_recv, 0);
            if (len > 0) {
                g_total_data += len;
                to_recv -= len;
            } else {
                show_socket_error_reason("recv_data", connect_socket);
                break;
            }
        }
 	  uint32_t GlobalCommand = (uint32_t) *databuff;
/*	  printf("The received value is: %zu \n", GlobalCommand);*/
	  if (!xQueueSend(xQueueGlobal, &GlobalCommand, portMAX_DELAY)) {
	  	printf("Failed to send to xQueueGlobal\n");	
        }	
/*	  vTaskResume(tx_task);*/
/*	  vTaskSuspend(NULL);*/
        
// Seems like the following code always break, that is why we cannot print anything after the if function.
	  if (g_total_data > 0) {
            continue;
        } else {
		printf("The while loop is broken");
            break;
        }
    }

    g_rxtx_need_restart = true;
    free(databuff);
    vTaskDelete(NULL);
}


//use this esp32 as a tcp server. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_server()
{
    ESP_LOGI(TAG, "server socket....port=%d\n", EXAMPLE_DEFAULT_PORT);
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        show_socket_error_reason("create_server", server_socket);
        return ESP_FAIL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(EXAMPLE_DEFAULT_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        show_socket_error_reason("bind_server", server_socket);
        close(server_socket);
        return ESP_FAIL;
    }
    if (listen(server_socket, 5) < 0) {
        show_socket_error_reason("listen_server", server_socket);
        close(server_socket);
        return ESP_FAIL;
    }
    connect_socket = accept(server_socket, (struct sockaddr *)&client_addr, &socklen);
    if (connect_socket < 0) {
        show_socket_error_reason("accept_server", connect_socket);
        close(server_socket);
        return ESP_FAIL;
    }
    /*connection established???now can send/recv*/
    ESP_LOGI(TAG, "tcp connection established!");
    return ESP_OK;
}
//use this esp32 as a tcp client. return ESP_OK:success ESP_FAIL:error
esp_err_t create_tcp_client()
{
    ESP_LOGI(TAG, "client socket....serverip:port=%s:%d\n",
             EXAMPLE_DEFAULT_SERVER_IP, EXAMPLE_DEFAULT_PORT);
    connect_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (connect_socket < 0) {
        show_socket_error_reason("create client", connect_socket);
        return ESP_FAIL;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(EXAMPLE_DEFAULT_PORT);
    server_addr.sin_addr.s_addr = inet_addr(EXAMPLE_DEFAULT_SERVER_IP);
    ESP_LOGI(TAG, "connecting to server...");
    if (connect(connect_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        show_socket_error_reason("client connect", connect_socket);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "connect to server success!");
    return ESP_OK;
}

//wifi_init_sta
void wifi_init_sta()
{
    tcp_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_DEFAULT_SSID,
            .password = EXAMPLE_DEFAULT_PWD
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s \n",
             EXAMPLE_DEFAULT_SSID, EXAMPLE_DEFAULT_PWD);
}
//wifi_init_softap
void wifi_init_softap()
{
    tcp_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_DEFAULT_SSID,
            .ssid_len = 0,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .password = EXAMPLE_DEFAULT_PWD,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_DEFAULT_PWD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s \n",
             EXAMPLE_DEFAULT_SSID, EXAMPLE_DEFAULT_PWD);
}




int get_socket_error_code(int socket)
{
    int result;
    u32_t optlen = sizeof(int);
    int err = getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen);
    if (err == -1) {
        ESP_LOGE(TAG, "getsockopt failed:%s", strerror(err));
        return -1;
    }
    return result;
}

int show_socket_error_reason(const char *str, int socket)
{
    int err = get_socket_error_code(socket);

    if (err != 0) {
        ESP_LOGW(TAG, "%s socket error %d %s", str, err, strerror(err));
    }

    return err;
}

int check_working_socket()
{
    int ret;
#if EXAMPLE_ESP_TCP_MODE_SERVER
    ESP_LOGD(TAG, "check server_socket");
    ret = get_socket_error_code(server_socket);
    if (ret != 0) {
        ESP_LOGW(TAG, "server socket error %d %s", ret, strerror(ret));
    }
    if (ret == ECONNRESET) {
        return ret;
    }
#endif
    ESP_LOGD(TAG, "check connect_socket");
    ret = get_socket_error_code(connect_socket);
    if (ret != 0) {
        ESP_LOGW(TAG, "connect socket error %d %s", ret, strerror(ret));
    }
    if (ret != 0) {
        return ret;
    }
    return 0;
}

void close_socket()
{
    close(connect_socket);
    close(server_socket);
}

