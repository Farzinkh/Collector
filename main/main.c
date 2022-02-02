// ADC
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_system.h"

// SD CARD
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
#include <dirent.h>
#include <inttypes.h>

// WIFI
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "freertos/event_groups.h"

// camera
#include <sys/param.h>
#include "esp_camera.h"

// server
#include "http_server.h"

// sleep
//#include "esp_sleep.h"
//#include "esp_timer.h"
//#define BUTTON_GPIO_NUM_DEFAULT     4
/* "Boot" button is active low */
//#define BUTTON_WAKEUP_LEVEL_DEFAULT     1

// camera config
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 // software reset will be performed
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

int PICSPEED = CONFIG_PICSPEED;
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

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = CONFIG_XCLK_FREQ,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = CONFIG_PIXFORMAT, // YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = CONFIG_FRAMESIZE,   // QQVGA-UXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, // 0-63 lower number means higher quality
    .fb_count = 1,      // if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera()
{
    // initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG3, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

// wifi
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG2 = "wifi";
static int s_retry_num = 0;



static void sta_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG2, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            esp_restart();
        }
        ESP_LOGI(TAG2,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG2, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
    ESP_LOGI(TAG2, "Event %d happend" ,event_id);
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    //static ip config

    //esp_netif_t *my_sta = esp_netif_create_default_wifi_sta();

    //esp_netif_dhcpc_stop(my_sta);

    //esp_netif_ip_info_t ip_info;

    //IP4_ADDR(&ip_info.ip, 192, 168, 43, 22);
   	//IP4_ADDR(&ip_info.gw, 192, 168, 15, 1);
   	//IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    //esp_netif_set_ip_info(my_sta, &ip_info);

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &sta_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG2, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG2, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG2, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
                 esp_restart();
    } else {
        ESP_LOGE(TAG2, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    //ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    //vEventGroupDelete(s_wifi_event_group);
}

static void AP_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG2, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG2, "station " MACSTR " leave, AID=%d",
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
                                                        &AP_wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG2, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

// server
#define STORAGE_NAMESPACE "www"
static const char *TAG4 = "server";
QueueHandle_t xQueueHttp;

void http_server_task(void *pvParameters);
// load key & value from NVS
esp_err_t load_key_value(char *key, char *value, size_t size)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    // Read
    size_t _size = size;
    err = nvs_get_str(my_handle, key, value, &_size);
    // if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
    if (err != ESP_OK)
        return err;

    // Close
    nvs_close(my_handle);
    // return ESP_OK;
    return err;
}

int find_values(char *key, char *parameter, char *value)
{
    // char * addr1;
    char *addr1 = strstr(parameter, key);
    if (addr1 == NULL)
        return 0;
    ESP_LOGD(TAG4, "addr1=%s", addr1);

    char *addr2 = addr1 + strlen(key);
    ESP_LOGD(TAG4, "addr2=[%s]", addr2);

    char *addr3 = strstr(addr2, "&");
    ESP_LOGD(TAG4, "addr3=%p", addr3);
    if (addr3 == NULL)
    {
        strcpy(value, addr2);
    }
    else
    {
        int length = addr3 - addr2;
        ESP_LOGD(TAG4, "addr2=%p addr3=%p length=%d", addr2, addr3, length);
        strncpy(value, addr2, length);
        value[length] = 0;
    }
    ESP_LOGI(TAG4, "key=[%s] value=[%s]", key, value);
    return strlen(value);
}

// esp_err_t save_key_values(char * key, char * value)
//{
//	nvs_handle_t my_handle;
//	esp_err_t err;

// Open
//	err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
//	if (err != ESP_OK) return err;

// Write
//	err = nvs_set_str(my_handle, key, value);
//	if (err != ESP_OK) return err;

// Commit written value.
// After setting any values, nvs_commit() must be called to ensure changes are written
// to flash storage. Implementations may write to storage at other times,
// but this is not guaranteed.
//	err = nvs_commit(my_handle);
//	if (err != ESP_OK) return err;

// Close
//	nvs_close(my_handle);
//	return ESP_OK;
//}

// sdcard
static const char *TAG = "sdcard";

#define MOUNT_POINT "/sdcard"

// This example can use SDMMC and SPI peripherals to communicate with SD card.
// By default, SDMMC peripheral is used.
// To enable SPI mode, uncomment the following line:

#define USE_SPI_MODE

// DMA channel to be used by the SPI peripheral
#ifndef SPI_DMA_CHAN
#define SPI_DMA_CHAN 1
#endif // SPI_DMA_CHAN

#ifdef USE_SPI_MODE
// Pin mapping when using SPI mode.
// With this mapping, SD card can be used both in SPI and 1-line SD mode.
// Note that a pull-up on CS line is required in SD mode.
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define PIN_NUM_MISO CONFIG_MISO
#define PIN_NUM_MOSI CONFIG_MOSI
#define PIN_NUM_CLK CONFIG_CLK
#define PIN_NUM_CS CONFIG_CS

#endif // CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#endif // USE_SPI_MODE

int search_in_sdcard(void)
{
    DIR *d;
    struct dirent *dir;
    d = opendir(MOUNT_POINT);
    ESP_LOGI(TAG, "============Listing files in sdcard============");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            ESP_LOGI(TAG, "%s", dir->d_name);
        }
        closedir(d);
    }
    return (0);
}

// adc
//#define ADC2_CHANNEL    CONFIG_ADC2_CHANNEL
//#if CONFIG_IDF_TARGET_ESP32
//#include "driver/sdmmc_host.h"
// static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
//#elif CONFIG_IDF_TARGET_ESP32S2
// static const adc_bits_width_t width = ADC_WIDTH_BIT_13;
//#endif
// static esp_adc_cal_characteristics_t *adc_chars;
// static const adc_atten_t atten = ADC_ATTEN_DB_11;
// static const adc_unit_t unit = ADC_UNIT_1;

// static void print_char_val_type(esp_adc_cal_value_t val_type)
//{
//     if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
//         printf("Characterized using Two Point Value\n");
//     } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
//         printf("Characterized using eFuse Vref\n");
//     } else {
//         printf("Characterized using Default Vref\n");
//     }
// }

void app_main(void)
{
    // Initialize SPIFFS for frontend files
    ESP_LOGI(TAG4, "Initializing SPIFFS");

    // Create Queue
    xQueueHttp = xQueueCreate(10, sizeof(URL_t));
    configASSERT(xQueueHttp);
    
    // Initialize NVS for wifi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    tcpip_adapter_ip_info_t ip_info;
    if (CONFIG_WIFI_MODE==0){
    ESP_LOGI(TAG2, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
    /* Get the local IP address */
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info));
    } else {
    ESP_LOGI(TAG2, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    /* Get the local IP address */
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    }
    bool wifi_is_on = true;

    char cparam0[64];
    sprintf(cparam0, "%s", ip4addr_ntoa(&ip_info.ip));
    xTaskCreate(http_server_task, "HTTP", 1024 * 6, (void *)cparam0, 2, NULL);

    // Wait for the task to start, because cparam0 is discarded.
    vTaskDelay(10);

    // server
    URL_t urlBuf;
    esp_err_t r;
    char val[16] = {0};
    char val2[16] = {0};
    char val3[16] = {0};
    char camera_state[16] = "1";
    // load key & value from NVS to check changes
    strcpy(urlBuf.url, "framesize");
    r = load_key_value(urlBuf.url, urlBuf.parameter, sizeof(urlBuf.parameter));
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG4, "Error (%s) loading to NVS", esp_err_to_name(r));
    }
    else
    {
        find_values("radio=", urlBuf.parameter, val);
        ESP_LOGI(TAG4, "Framesize readed config: %s", val);
        camera_config.frame_size = atoi(val);
    }
    strcpy(urlBuf.url, "pixformat");
    r = load_key_value(urlBuf.url, urlBuf.parameter, sizeof(urlBuf.parameter));
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG4, "Error (%s) loading to NVS", esp_err_to_name(r));
    }
    else
    {
        find_values("radio2=", urlBuf.parameter, val2);
        ESP_LOGI(TAG4, "Pixformat readed config: %s", val2);
        camera_config.pixel_format = atoi(val2);
    }
    strcpy(urlBuf.url, "picspeed");
    r = load_key_value(urlBuf.url, urlBuf.parameter, sizeof(urlBuf.parameter));
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG4, "Error (%s) loading to NVS", esp_err_to_name(r));
    }
    else
    {
        find_values("radio3=", urlBuf.parameter, val3);
        ESP_LOGI(TAG4, "Picspeed readed config: %s", val3);
        PICSPEED = atoi(val3);
    }
    strcpy(urlBuf.url, "camera");
    r = load_key_value(urlBuf.url, urlBuf.parameter, sizeof(urlBuf.parameter));
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG4, "Error (%s) loading to NVS", esp_err_to_name(r));
    }
    else
    {
        ESP_LOGI(TAG4, "Camera state readed config: %s", urlBuf.parameter);
        strcpy(camera_state, urlBuf.parameter);
    }
    // Init camera
    if (ESP_OK != init_camera())
    {
        return;
    }

    // init adc
    // uint8_t output_data=0;
    // int     read_raw;
    // gpio_num_t adc_gpio_num;

    // r = adc2_pad_get_io_num( ADC2_CHANNEL, &adc_gpio_num );
    // assert( r == ESP_OK );

    // ADC refrence
    // int DEFAULT_VREF=adc2_vref_to_gpio(adc_gpio_num);         //Use adc2_vref_to_gpio() to obtain a better estimate

    // Characterize ADC
    // adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    // esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    // print_char_val_type(val_type);

    // printf("ADC2 channel %d @ GPIO %d.\n", ADC2_CHANNEL, adc_gpio_num);

    // be sure to do the init before using adc2.
    // printf("adc2_init...\n");
    // adc2_config_channel_atten( ADC2_CHANNEL, ADC_ATTEN_11db );

    // vTaskDelay(2 * portTICK_PERIOD_MS);

    // printf("start conversion.\n");

    // SDcard
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};
    sdmmc_card_t *card;
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
    r = spi_bus_initialize(host.slot, &bus_cfg, SPI_DMA_CHAN);
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    r = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (r != ESP_OK)
    {
        if (r == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(r));
        }
        return;
    }

    // Card has been initialized, print its properties
    // search_in_sdcard(); // list files
    sdmmc_card_print_info(stdout, card);

    // time
    time_t now;
    struct tm *tm;
    char filename[255];
    char fullname[255];
    // sleep config
    /* Configure the button GPIO as input, enable wakeup */
    // const int button_gpio_num = BUTTON_GPIO_NUM_DEFAULT;
    // const int wakeup_level = BUTTON_WAKEUP_LEVEL_DEFAULT;
    // gpio_config_t config = {
    //         .pin_bit_mask = BIT64(button_gpio_num),
    //         .mode = GPIO_MODE_INPUT
    // };
    // ESP_ERROR_CHECK(gpio_config(&config));
    // gpio_wakeup_enable(button_gpio_num,
    //         wakeup_level == 0 ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    // gpio_set_level(button_gpio_num,0);
    while (1)
    {
        if ((wifi_is_on == true) & (strcmp(camera_state, "1") == 0))
        {
            ESP_LOGI(TAG3, "Taking picture...");
            camera_fb_t *pic = esp_camera_fb_get();
            if (!pic)
            {
                ESP_LOGE(TAG3, "Camera capture failed");
                break;
            }
            // use pic->buf to access the image
            ESP_LOGI(TAG3, "Picture taken! Its size was: %zu bytes", pic->len);

            // Use POSIX and C standard library functions to work with files.
            // First create a file.
            ESP_LOGI(TAG, "Opening file");

            now = time(0);        // get current time
            tm = localtime(&now); // get structure

            // fmt2jpg(pic->buf, pic->len, pic->width, pic->height, pic->format, quality, outframe.buf, outframe.len);
            if (camera_config.pixel_format == 3)
            {
                memset(fullname, 0, sizeof(fullname));
                sprintf(filename, "/%02d%02d%02d%02d.jpg", tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
                strcat(fullname, MOUNT_POINT);
                strcat(fullname, filename);
                FILE *f = fopen(fullname, "wb");
                if (f == NULL)
                {
                    ESP_LOGE(TAG, "Failed to open file for writing");
                    ESP_LOGE(TAG, "%s", fullname);
                    return;
                }
                fwrite(pic->buf, 1, pic->len, f);
                fclose(f);
            } else {
                memset(fullname, 0, sizeof(fullname));
                sprintf(filename, "/%02d%02d%02d%02d.raw", tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
                strcat(fullname, MOUNT_POINT);
                strcat(fullname, filename);
                FILE *f = fopen(fullname, "wb");
                if (f == NULL)
                {
                    ESP_LOGE(TAG, "Failed to open file for writing");
                    ESP_LOGE(TAG, "%s", fullname);
                    return;
                }
                fwrite(pic->buf, 1, pic->len, f);
                fclose(f);

            }
            ESP_LOGI(TAG, "File written");

            // release buffer
            esp_camera_fb_return(pic);
            vTaskDelay(PICSPEED / portTICK_RATE_MS);
        }
        else
        {

            // adc
            // r = adc2_get_raw( ADC2_CHANNEL, width, &read_raw);
            // uint32_t voltage = esp_adc_cal_raw_to_voltage(read_raw, adc_chars);
            // if ( r == ESP_OK ) {
            //     //printf("%d: \n", read_raw );
            //     printf("Raw: %d\tVoltage: %dmV\n", read_raw, voltage);
            // } else if ( r == ESP_ERR_INVALID_STATE ) {
            //     printf("%s: ADC2 not initialized yet.\n", esp_err_to_name(r));
            // } else if ( r == ESP_ERR_TIMEOUT ) {
            //     //This can not happen in this example. But if WiFi is in use, such error code could be returned.
            //     printf("%s: ADC2 is in use by Wi-Fi.\n", esp_err_to_name(r));
            // } else {
            //     printf("%s\n", esp_err_to_name(r));
            // }

            // vTaskDelay( 2 * portTICK_PERIOD_MS );

            // Entering light sleep
            // esp_sleep_enable_timer_wakeup(10000000);
            // esp_sleep_enable_gpio_wakeup();
            /* Wait until GPIO goes high */
            // if (gpio_get_level(button_gpio_num) == wakeup_level) {
            //     ESP_LOGI(TAG4,"Waiting for GPIO %d to go high...\n", button_gpio_num);
            //     do {
            //         vTaskDelay(pdMS_TO_TICKS(10));
            //     } while (gpio_get_level(button_gpio_num) == wakeup_level);
            // }

            /* Get timestamp before entering sleep */
            // int64_t t_before_us = esp_timer_get_time();
            /* Enter sleep mode */
            // esp_light_sleep_start();
            // int64_t t_after_us = esp_timer_get_time();
            // ESP_LOGI(TAG3, "Camera is oFF slept for %lld ms",(t_after_us - t_before_us) / 1000);
            /* Determine wake up reason */
            // switch (esp_sleep_get_wakeup_cause()) {
            //     case ESP_SLEEP_WAKEUP_TIMER:
            //         break;
            //     case ESP_SLEEP_WAKEUP_GPIO:
            //         r = save_key_values("camera", "1");
            //		if (r != ESP_OK) {
            //			ESP_LOGE(TAG4, "Error (%s) saving to NVS", esp_err_to_name(r));
            //		} else {
            //		    ESP_LOGI(TAG3, "Exiting from sleep");
            //		}
            //		esp_restart();
            //         break;
            //     default:
            //         break;
            // }
            ESP_LOGI(TAG3, "Camera is oFF");
            vTaskDelay(10000000 / portTICK_RATE_MS);
        }
    }
}
