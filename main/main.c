//ADC
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_system.h"

//SD CARD
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"

//WIFI
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

//camera
#include <sys/param.h>
#include "esp_camera.h"

#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

static const char *TAG3 = "camera";

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sscb_sda = CAM_PIN_SIOD,
    .pin_sscb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = CONFIG_XCLK_FREQ,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = CONFIG_PIXFORMAT, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = CONFIG_FRAMESIZE,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 1,       //if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera()
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG3, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

#define ADC2_CHANNEL    CONFIG_ADC2_CHANNEL
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN       CONFIG_ESP_MAX_STA_CONN

//wifi
static const char *TAG2 = "wifi softAP";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG2, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG2, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG2, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

//sdcard
static const char *TAG = "sdcard";

#define MOUNT_POINT "/sdcard"

// This example can use SDMMC and SPI peripherals to communicate with SD card.
// By default, SDMMC peripheral is used.
// To enable SPI mode, uncomment the following line:

#define USE_SPI_MODE

// DMA channel to be used by the SPI peripheral
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN    1
#endif //SPI_DMA_CHAN

#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define PIN_NUM_MISO CONFIG_MISO
#define PIN_NUM_MOSI CONFIG_MOSI
#define PIN_NUM_CLK  CONFIG_CLK
#define PIN_NUM_CS   CONFIG_CS

#endif //CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#endif //USE_SPI_MODE

//adc
//#if CONFIG_IDF_TARGET_ESP32
//#include "driver/sdmmc_host.h"
//static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
//#elif CONFIG_IDF_TARGET_ESP32S2
//static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
//#endif
//static esp_adc_cal_characteristics_t *adc_chars;
//static const adc_atten_t atten = ADC_ATTEN_DB_11;
//static const adc_unit_t unit = ADC_UNIT_1;

//static void print_char_val_type(esp_adc_cal_value_t val_type)
//{
//    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
//        printf("Characterized using Two Point Value\n");
//    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
//        printf("Characterized using eFuse Vref\n");
//    } else {
//        printf("Characterized using Default Vref\n");
//    }
//}

void app_main(void)
{
    //Init camera
    if(ESP_OK != init_camera()) {
        return;
    }
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG2, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    bool wifi_is_on =true;
    esp_err_t r;
    
    // init adc
    //uint8_t output_data=0; 
    //int     read_raw;
    //gpio_num_t adc_gpio_num;
    
    //r = adc2_pad_get_io_num( ADC2_CHANNEL, &adc_gpio_num );
    //assert( r == ESP_OK );

    //ADC refrence
    //int DEFAULT_VREF=adc2_vref_to_gpio(adc_gpio_num);         //Use adc2_vref_to_gpio() to obtain a better estimate
    
    //Characterize ADC
    //adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    //esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    //print_char_val_type(val_type);
    
    //printf("ADC2 channel %d @ GPIO %d.\n", ADC2_CHANNEL, adc_gpio_num);


    //be sure to do the init before using adc2.
    //printf("adc2_init...\n");
    //adc2_config_channel_atten( ADC2_CHANNEL, ADC_ATTEN_11db );

    //vTaskDelay(2 * portTICK_PERIOD_MS);

    //printf("start conversion.\n");
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    #ifdef CONFIG_FORMAT_IF_MOUNT_FAILED
            .format_if_mount_failed = true,
    #else
            .format_if_mount_failed = false,
    #endif // FORMAT_IF_MOUNT_FAILED
            .max_files = 5,
            .allocation_unit_size = 16 * 1024
        };
        sdmmc_card_t* card;
        const char mount_point[] = MOUNT_POINT;
        ESP_LOGI(TAG, "Initializing SD card");
    
        ESP_LOGI(TAG, "Using SPI peripheral");

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    r= spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    r = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);
    
    if (r != ESP_OK) {
        if (r == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                "If you want the card to be formatted, set the FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(r));
        }
        return;
    }

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);
    
    while(1) {
        if (wifi_is_on==true){
            ESP_LOGI(TAG3, "Taking picture...");
            camera_fb_t *pic = esp_camera_fb_get();

            // use pic->buf to access the image
            ESP_LOGI(TAG3, "Picture taken! Its size was: %zu bytes", pic->len);
            esp_camera_fb_return(pic);

            vTaskDelay(5000 / portTICK_RATE_MS);
        } else {
        
            //adc 
            //r = adc2_get_raw( ADC2_CHANNEL, width, &read_raw);
            //uint32_t voltage = esp_adc_cal_raw_to_voltage(read_raw, adc_chars);
            //if ( r == ESP_OK ) {
            //    //printf("%d: \n", read_raw );
            //    printf("Raw: %d\tVoltage: %dmV\n", read_raw, voltage);
            //} else if ( r == ESP_ERR_INVALID_STATE ) {
            //    printf("%s: ADC2 not initialized yet.\n", esp_err_to_name(r));
            //} else if ( r == ESP_ERR_TIMEOUT ) {
            //    //This can not happen in this example. But if WiFi is in use, such error code could be returned.
            //    printf("%s: ADC2 is in use by Wi-Fi.\n", esp_err_to_name(r));
            //} else {
            //    printf("%s\n", esp_err_to_name(r));
            //}

            //vTaskDelay( 2 * portTICK_PERIOD_MS );
        }
    }
}
