#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_tls.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include <driver/gpio.h>

#include "WiFi_Sta.h"
#include "WiFi_Secure.h"
#include "ESP32_GPIO.h"
#include <dht.h>

/* DHT11 Defines */
#define DHT11_PIN               17
#define BUZZER_PIN              18
#define BUZZER_PIN_SEL          (1ULL << BUZZER_PIN)
#define HUMIDITY_THRESHOLD      800
#define TEMPERATURE_THRESHOLD   150

/* User PV Functions */
static void init_hw(void);
static void check_alarm(void);

/* User PV Task Functions */
static void beep(void *args);
static void dht11Task(void *args);

/* User PV Variables */
static int16_t temperature;
static int16_t humidity;
static bool temperature_alarm = false;
static bool humidity_alarm = false;


static const char *TAG = "THINGS_EXAMPLE";

void app_main(){

    ESP_LOGI(TAG, "[APP] Startup..");
    
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // esp_log_level_set("*", ESP_LOG_INFO);
    // esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    // esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    // esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    // esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    // esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    // esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    init_hw();

    int app_cpu = xPortGetCoreID();
    ESP_LOGI(TAG, "app_cpu is %d (%s core)\n", app_cpu, app_cpu > 0 ? "Dual" : "Single");

    xTaskCreatePinnedToCore(dht11Task, "dht11Task", 8192, NULL, 5, NULL, app_cpu);
}

/* User PV Functions */
static void init_hw(void){
    ESP32_GPIO_init_output(BUZZER_PIN);
}

static void check_alarm(void){
    bool is_alarm = temperature >= TEMPERATURE_THRESHOLD;
    bool run_beep = is_alarm && !temperature_alarm;
    temperature_alarm = is_alarm;

    if(run_beep){
        xTaskCreate(beep, "beep", configMINIMAL_STACK_SIZE, (void*)3, 5, NULL);
        temperature_alarm = false;
        return;
    }

    is_alarm = humidity >= HUMIDITY_THRESHOLD;
    run_beep = is_alarm && !humidity_alarm;
    humidity_alarm = is_alarm;

    if(run_beep){
        xTaskCreate(beep, "beep", configMINIMAL_STACK_SIZE, (void*)2, 5, NULL);
        humidity_alarm = false;
        return;
    }

}

/* User PV Task Functions */
static void beep(void *args){

    int cnt = 2 * (int)args;
    bool state = true;

    for(int i = 0; i < cnt; ++i, state = !state){
        gpio_set_level(BUZZER_PIN, state);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    vTaskDelete(NULL);
    
}

static void dht11Task(void *args){


    while (1) {

        if(dht_read_data(DHT_TYPE_DHT11, (gpio_num_t)DHT11_PIN, &humidity, &temperature) == ESP_OK){
            
            check_alarm();
            xTaskCreatePinnedToCore(https_get_task, "ThingsTask", 8192, NULL, 5, NULL, 0);
            ESP_LOGI(TAG, "temperature=%d, humidity=%d", temperature, humidity);
        }
        else{
            ESP_LOGI(TAG, "could not read data from dht11 sensor");
        }

        /**
         * The sampling rate of DHT11 should not exceed 1 Hz (that is, one reading per
         * second). Otherwise, the sensor gets hot and the readings will be inaccurate.
         * 
         */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}