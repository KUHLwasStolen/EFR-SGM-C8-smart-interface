#include <stdio.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#define WIFI_MAXIMUM_RETRY 5
uint8_t wifi_retries = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

httpd_handle_t serverHandle = NULL;
static EventGroupHandle_t s_wifi_event_group;

extern const uint8_t indexHtmlFile[] asm("_binary_index_html_start");
extern const uint8_t AP_SSID[] asm("_binary_YOUR_AP_SSID_txt_start");
extern const uint8_t AP_PASSWORD[] asm("_binary_YOUR_AP_PASSWORD_txt_start");
extern const uint8_t faviconPNG_start[] asm("_binary_favicon_png_start");
extern const uint8_t faviconPNG_end[] asm("_binary_favicon_png_end");

int16_t currentPower = 0, currentPowerL1 = 0, currentPowerL2 = 0, currentPowerL3 = 0;

// To decode the SML messages
static void irReaderTask(void* args) {
    const uart_port_t irUART = UART_NUM_1;
    uart_config_t irConfig = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_RTS,
        .rx_flow_ctrl_thresh = 122,
    };

    ESP_ERROR_CHECK(uart_param_config(irUART, &irConfig));
    ESP_ERROR_CHECK(uart_set_pin(irUART, 21, 18, 22, 23));

    const int uart_buffer_size = 1200 * 2;
    QueueHandle_t uart_queue;

    ESP_ERROR_CHECK(uart_driver_install(irUART, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
    // ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(irUART, 0x1B, 4, 4, 0, 50));

    uint8_t buffer[uart_buffer_size] = {};
    uint16_t readLength = 0;
    uint8_t powerID;
    uint8_t powerLength;

    while(1) {
        readLength = uart_read_bytes(irUART, buffer, uart_buffer_size, 300 / portTICK_PERIOD_MS);

        if(readLength > 0) {
            ESP_LOGI("UART", "New Message (size: %u):", readLength);

            powerID = 0;
            powerLength = 0;

            for(int i = 0; i < readLength; i++) {

                // Power:   77 07 01 00 10 07 00 ff
                // L1:      77 07 01 00 24 07 00 ff
                // L2:      77 07 01 00 38 07 00 ff
                // L3:      77 07 01 00 4c 07 00 ff
                if(buffer[i] == 0x77) {
                    if(buffer[i+1] == 0x07) {
                        if(buffer[i+2] == 0x01) {
                            if(buffer[i+3] == 0x00) {
                                if(buffer[i+4] == 0x10) { // overall power
                                    if(buffer[i+5] == 0x07) {
                                        if(buffer[i+6] == 0x00) {
                                            if(buffer[i+7] == 0xFF) {
                                                powerID = buffer[i+13];
                                                powerLength = ((uint8_t)(powerID << 4)) >> 4;
                                                
                                                switch(powerLength) {
                                                    case 1: 
                                                        currentPower = 0;
                                                    break;

                                                    case 2:
                                                        currentPower = (int16_t)(int8_t)buffer[i+14];
                                                    break;

                                                    case 3:
                                                        currentPower = (int16_t)(((uint16_t)(buffer[i+14]) << 8) + (uint16_t)buffer[i+15]);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                } else if(buffer[i+4] == 0x24) { // L1
                                    if(buffer[i+5] == 0x07) {
                                        if(buffer[i+6] == 0x00) {
                                            if(buffer[i+7] == 0xFF) {
                                                powerID = buffer[i+13];
                                                powerLength = ((uint8_t)(powerID << 4)) >> 4;
                                                
                                                switch(powerLength) {
                                                    case 1: 
                                                        currentPowerL1 = 0;
                                                    break;

                                                    case 2:
                                                        currentPowerL1 = (int16_t)(int8_t)buffer[i+14];
                                                    break;

                                                    case 3:
                                                        currentPowerL1 = (int16_t)(((uint16_t)(buffer[i+14]) << 8) + (uint16_t)buffer[i+15]);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                } else if(buffer[i+4] == 0x38) { // L2
                                    if(buffer[i+5] == 0x07) {
                                        if(buffer[i+6] == 0x00) {
                                            if(buffer[i+7] == 0xFF) {
                                                powerID = buffer[i+13];
                                                powerLength = ((uint8_t)(powerID << 4)) >> 4;
                                                
                                                switch(powerLength) {
                                                    case 1: 
                                                        currentPowerL2 = 0;
                                                    break;

                                                    case 2:
                                                        currentPowerL2 = (int16_t)(int8_t)buffer[i+14];
                                                    break;

                                                    case 3:
                                                        currentPowerL2 = (int16_t)(((uint16_t)(buffer[i+14]) << 8) + (uint16_t)buffer[i+15]);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                } else if(buffer[i+4] == 0x4C) { // L3
                                    if(buffer[i+5] == 0x07) {
                                        if(buffer[i+6] == 0x00) {
                                            if(buffer[i+7] == 0xFF) {
                                                powerID = buffer[i+13];
                                                powerLength = ((uint8_t)(powerID << 4)) >> 4;
                                                
                                                switch(powerLength) {
                                                    case 1: 
                                                        currentPowerL3 = 0;
                                                    break;

                                                    case 2:
                                                        currentPowerL3 = (int16_t)(int8_t)buffer[i+14];
                                                    break;

                                                    case 3:
                                                        currentPowerL3 = (int16_t)(((uint16_t)(buffer[i+14]) << 8) + (uint16_t)buffer[i+15]);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ESP_LOGI("Power", "Overall: %d  L1: %d  L2: %d  L3: %d\n", currentPower, currentPowerL1, currentPowerL2, currentPowerL3);

            ESP_ERROR_CHECK(uart_flush_input(irUART));
        }
    }
}

// From here on everything related to the web server
// handles http-GET requests
esp_err_t GET_handler(httpd_req_t *req) {    
    httpd_resp_send(req, (char*)indexHtmlFile, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/\"");
    return ESP_OK;
}

// URI handler structure for GET "/"
httpd_uri_t uri_GET = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = GET_handler,
    .user_ctx = NULL
};

esp_err_t GET_handler_api_power(httpd_req_t *req) {
    char resp[64];
    sprintf(resp, "{\"power\":%hi, \"l1\":%hi, \"l2\":%hi, \"l3\":%hi}", currentPower, currentPowerL1, currentPowerL2, currentPowerL3);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/api/power\"");
    return ESP_OK;
}

// URI handler structure for GET "/api/power"
httpd_uri_t uri_GET_api_power = {
    .uri      = "/api/power",
    .method   = HTTP_GET,
    .handler  = GET_handler_api_power,
    .user_ctx = NULL
};

esp_err_t GET_handler_favicon(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, (char*)faviconPNG_start, faviconPNG_end - faviconPNG_start);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/favicon.ico\"");
    return ESP_OK;
}

// URI handler structure for GET "/favicon.ico
httpd_uri_t uri_GET_favicon = {
    .uri      = "/favicon.ico",
    .method   = HTTP_GET,
    .handler  = GET_handler_favicon,
    .user_ctx = NULL
};

// Final server set up and start
httpd_handle_t start_webserver() {
    // Generate default config
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Empty handle to esp_http_server
    httpd_handle_t server = NULL;

    // Start the httpd server
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_GET);
        httpd_register_uri_handler(server, &uri_GET_api_power);
        httpd_register_uri_handler(server, &uri_GET_favicon);
    }

    // handle == NULL if start failed
    return server;
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retries < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            wifi_retries++;
            ESP_LOGI("WIFI", "Connection to AP failed. Retrying...");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        
        ESP_LOGE("WIFI", "Connection to the AP fail");

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI", "IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retries = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // init wifi config
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    // overwrite SSID and PASSWORD with actual values
    strcpy((char *)wifi_config.sta.ssid, (char *)AP_SSID);
	strcpy((char *)wifi_config.sta.password, (char *)AP_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI("WIFI", "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("WIFI", "Successfully connected to AP with SSID: %s", (char*)AP_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI("WIFI", "Failed to connect to AP with SSID: %s", (char*)AP_SSID);
    } else {
        ESP_LOGE("WIFI", "UNEXPECTED EVENT");
    }
}

static void httpServerTask(void* args) {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    serverHandle = start_webserver();

    if(serverHandle == NULL) {
        ESP_LOGE("HTTP server", "Failed to start server! Restarting...");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        esp_restart();
    }

    ESP_LOGI("HTTP server", "Successfully started server!");

    while(1) {
        vTaskDelay(1);
    }
}

void app_main(void) {
    xTaskCreatePinnedToCore(irReaderTask, "irReaderTask", 10000, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(httpServerTask, "httpServerTask", 10000, NULL, 10, NULL, 0);
}