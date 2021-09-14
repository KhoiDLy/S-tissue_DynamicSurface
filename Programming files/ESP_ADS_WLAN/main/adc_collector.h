#ifndef __ADC_COLLECTOR_H__
#define __ADC_COLLECTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#define WINDOW_SIZE 2
#define NUM_CHANN 2

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_DRDY 27
#define PIN_NUM_RST  13
#define PIN_NUM_LED  25	

//uint32_t buffer[WINDOW_SIZE*NUM_CHANN];
uint32_t buffer[NUM_CHANN];

int buffer_idx;
bool buffer_full;

// volatile bool packet_ready;

static intr_handle_t s_timer_handle;
extern SemaphoreHandle_t xSemaphore;

extern QueueHandle_t xQueueGlobal;

volatile uint32_t timer_click_count;

//esp_adc_cal_characteristics_t adc_characteristics;

void init_timer(int timer_period_us);
void init_adcs();
void ADS1256_init();
void ADS1256_Collect();

#endif
