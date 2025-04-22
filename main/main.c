#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "soc/uart_channel.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "cJSON.h"

#define WIFI_MAXIMUM_RETRY 5
uint8_t wifi_retries = 0;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

httpd_handle_t serverHandle = NULL;
static EventGroupHandle_t s_wifi_event_group;

void insertFloatAtFirstIndex(float* arr, uint16_t len, float value);
void insertI32AtFirstIndex(int32_t* arr, uint16_t len, int32_t value);
float medianFloat(float* arr, uint16_t len);
int32_t medianI32(int32_t* arr, uint16_t len);
int compareFloat(const void* f1, const void* f2);
int compareI32(const void* i1, const void* i2);
void addFloatToMedianArr(float* arr, uint8_t* sizeCounter, uint8_t sizeMax, float value);
void addI32ToMedianArr(int32_t* arr, uint8_t* sizeCounter, uint8_t sizeMax, int32_t value);

extern const uint8_t indexHtmlFile[] asm("_binary_index_html_start");
extern const uint8_t stylesCssFile[] asm("_binary_styles_css_start");
extern const uint8_t updaterJsFile[] asm("_binary_updater_js_start");
extern const uint8_t settingsHtmlFile[] asm("_binary_settings_html_start");
extern const uint8_t settingsJsFile[] asm("_binary_settings_js_start");
extern const uint8_t faviconSVG_start[] asm("_binary_favicon_svg_start");
extern const uint8_t faviconSVG_end[] asm("_binary_favicon_svg_end");

typedef struct settings_t {
    float importCost;
    float importCostT1;
    float importCostT2;
    float exportCost;
    float exportCostT1;
    float exportCostT2;
    char currency[8];
} settings_t;

int32_t currentPower = 0, currentPowerL1 = 0, currentPowerL2 = 0, currentPowerL3 = 0; // in W
float importOverall = 0.0f, importT1 = 0.0f, importT2 = 0.0f, exportOverall = 0.0f, exportT1 = 0.0f, exportT2 = 0.0f; // in kWh
settings_t settings = {0};

time_t upSince = 0;
char upSinceStr[64] = {};
struct tm timeinfo = {0};

// To decode the SML messages
static void irReaderTask(void* args) {
    const uart_port_t irUART = UART_GPIO16_DIRECT_CHANNEL;
    uart_config_t irConfig = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(irUART, &irConfig));
    ESP_ERROR_CHECK(uart_set_pin(irUART, UART_PIN_NO_CHANGE, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    const int uart_buffer_size = 1200 * 2;
    QueueHandle_t uart_queue;

    ESP_ERROR_CHECK(uart_driver_install(irUART, uart_buffer_size, 0, 1, &uart_queue, 0));

    // All of these values are temporary and used by multiple code segments!!!
    // Only use them as placeholders for extracting information from the SML messages!!!
    uint8_t buffer[uart_buffer_size] = {};
    uint16_t readLength = 0;
    uint8_t valueID;
    uint8_t valueLength;
    int32_t tempPower;
    uint64_t meterReadRaw = 0;

    // defines how many values are saved at maximum, for the calculation of the median of the meter readings
    #define BIG_MEDIAN_SIZE_MAX 15
    float importMedianArr[BIG_MEDIAN_SIZE_MAX] = {};
    float importT1MedianArr[BIG_MEDIAN_SIZE_MAX] = {};
    float importT2MedianArr[BIG_MEDIAN_SIZE_MAX] = {};
    float exportMedianArr[BIG_MEDIAN_SIZE_MAX] = {};
    float exportT1MedianArr[BIG_MEDIAN_SIZE_MAX] = {};
    float exportT2MedianArr[BIG_MEDIAN_SIZE_MAX] = {};
    uint8_t importMedianSize = 0, importT1MedianSize = 0, importT2MedianSize = 0, exportMedianSize = 0, exportT1MedianSize = 0, exportT2MedianSize = 0;

    #define SMALL_MEDIAN_SIZE_MAX 5
    int32_t currentPowerMedianArr[SMALL_MEDIAN_SIZE_MAX] = {};
    int32_t currentPowerL1MedianArr[SMALL_MEDIAN_SIZE_MAX] = {};
    int32_t currentPowerL2MedianArr[SMALL_MEDIAN_SIZE_MAX] = {};
    int32_t currentPowerL3MedianArr[SMALL_MEDIAN_SIZE_MAX] = {};
    uint8_t currentPowerMedianSize = 0, currentPowerL1MedianSize = 0, currentPowerL2MedianSize = 0, currentPowerL3MedianSize = 0;

    while(1) {
        readLength = uart_read_bytes(irUART, buffer, uart_buffer_size - 1, 300 / portTICK_PERIOD_MS);

        if(readLength > 0) {
            ESP_LOGI("UART", "New Message (size: %u):", readLength);

            valueID = 0;
            valueLength = 0;

            for(uint16_t i = 0; i < readLength; i++) {
                //printf("%02X ", buffer[i]);

                // Power:           77 07 01 00 10 07 00 FF
                // L1:              77 07 01 00 24 07 00 FF
                // L2:              77 07 01 00 38 07 00 FF
                // L3:              77 07 01 00 4C 07 00 FF
                // Import overall:  77 07 01 00 01 08 00 FF
                // Import T1:       77 07 01 00 01 08 01 FF
                // Import T2:       77 07 01 00 01 08 02 FF
                // Export overall:  77 07 01 00 02 08 00 FF
                // Export T1:       77 07 01 00 02 08 01 FF
                // Export T2:       77 07 01 00 02 08 02 FF
                if(buffer[i] == 0x77) {
                    if(buffer[i+1] == 0x07) {
                        if(buffer[i+2] == 0x01) {
                            if(buffer[i+3] == 0x00) {
                                if(buffer[i+4] == 0x10 || buffer[i+4] == 0x24 || buffer[i+4] == 0x38 || buffer[i+4] == 0x4C) { // power values
                                    if(buffer[i+5] == 0x07) {
                                        if(buffer[i+6] == 0x00) {
                                            if(buffer[i+7] == 0xFF) {
                                                valueID = buffer[i+13];
                                                valueLength = ((uint8_t)(valueID << 4)) >> 4;
                                                tempPower = 0;
                                                
                                                switch(valueLength) {
                                                    case 2:
                                                        tempPower = (int32_t)(int8_t)buffer[i+14];
                                                    break;

                                                    case 3:
                                                        tempPower = ((int32_t)((int8_t)buffer[i+14]) << 8) + (uint32_t)buffer[i+15];
                                                    break;

                                                    case 4:
                                                        tempPower = ((int32_t)((int8_t)buffer[i+14]) << 16) + ((uint32_t)(buffer[i+15]) << 8) + (uint32_t)buffer[i+16];
                                                    break;
                                                }

                                                switch(buffer[i+4]) {
                                                    case 0x10:
                                                        addI32ToMedianArr(currentPowerMedianArr, &currentPowerMedianSize, SMALL_MEDIAN_SIZE_MAX, tempPower);
                                                        currentPower = medianI32(currentPowerMedianArr, currentPowerMedianSize);
                                                    break;

                                                    case 0x24:
                                                        addI32ToMedianArr(currentPowerL1MedianArr, &currentPowerL1MedianSize, SMALL_MEDIAN_SIZE_MAX, tempPower);
                                                        currentPowerL1 = medianI32(currentPowerL1MedianArr, currentPowerL1MedianSize);
                                                    break;

                                                    case 0x38:
                                                        addI32ToMedianArr(currentPowerL2MedianArr, &currentPowerL2MedianSize, SMALL_MEDIAN_SIZE_MAX, tempPower);
                                                        currentPowerL2 = medianI32(currentPowerL2MedianArr, currentPowerL2MedianSize);
                                                    break;

                                                    case 0x4C:
                                                        addI32ToMedianArr(currentPowerL3MedianArr, &currentPowerL3MedianSize, SMALL_MEDIAN_SIZE_MAX, tempPower);
                                                        currentPowerL3 = medianI32(currentPowerL3MedianArr, currentPowerL3MedianSize);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                } else if(buffer[i+4] == 0x01 || buffer[i+4] == 0x02) { // import, export
                                    if(buffer[i+5] == 0x08) {
                                        if(buffer[i+6] == 0x00 || buffer[i+6] == 0x01 || buffer[i+6] == 0x02) {
                                            if(buffer[i+7] == 0xFF) {
                                                // search for 0x52FF
                                                for(uint16_t j = i+8; j < i+30; j++) {
                                                    if(buffer[j] == 0x52 && buffer[j+1] == 0xFF) {
                                                        meterReadRaw = 0;

                                                        // search for next message to determine value length
                                                        for(uint8_t k = 2; k < 6; k++) {
                                                            if(buffer[j+3+k] == 0x01 && buffer[j+3+k+1] == 0x77) {
                                                                if(k < 3) break;

                                                                // now fill meterReadRaw according to length
                                                                for(uint8_t l = 0; l < k; l++) {
                                                                    meterReadRaw += ((uint64_t)buffer[j+3+l]) << ((k-1-l)*8);
                                                                }

                                                                switch(buffer[i+4]) {
                                                                    case 0x01: // import
                                                                        switch(buffer[i+6]) {
                                                                            case 0x00:
                                                                                addFloatToMedianArr(importMedianArr, &importMedianSize, BIG_MEDIAN_SIZE_MAX, meterReadRaw / 10000.0f);
                                                                                importOverall = medianFloat(importMedianArr, importMedianSize);
                                                                            break;

                                                                            case 0x01:
                                                                                addFloatToMedianArr(importT1MedianArr, &importT1MedianSize, BIG_MEDIAN_SIZE_MAX, meterReadRaw / 10000.0f);
                                                                                importT1 = medianFloat(importT1MedianArr, importT1MedianSize);
                                                                                if(importT1 > 1 && importT2 > 1) addFloatToMedianArr(importMedianArr, &importMedianSize, BIG_MEDIAN_SIZE_MAX, importT1 + importT2);
                                                                            break;

                                                                            case 0x02:
                                                                                addFloatToMedianArr(importT2MedianArr, &importT2MedianSize, BIG_MEDIAN_SIZE_MAX, meterReadRaw / 10000.0f);
                                                                                importT2 = medianFloat(importT2MedianArr, importT2MedianSize);
                                                                                if(importT1 > 1 && importT2 > 1) addFloatToMedianArr(importMedianArr, &importMedianSize, BIG_MEDIAN_SIZE_MAX, importT1 + importT2);
                                                                            break;
                                                                        }
                                                                    break;

                                                                    case 0x02: // export
                                                                        switch(buffer[i+6]) {
                                                                            case 0x00:
                                                                                addFloatToMedianArr(exportMedianArr, &exportMedianSize, BIG_MEDIAN_SIZE_MAX, meterReadRaw / 10000.0f);
                                                                                exportOverall = medianFloat(exportMedianArr, exportMedianSize);
                                                                            break;

                                                                            case 0x01:
                                                                                addFloatToMedianArr(exportT1MedianArr, &exportT1MedianSize, BIG_MEDIAN_SIZE_MAX, meterReadRaw / 10000.0f);
                                                                                exportT1 = medianFloat(exportT1MedianArr, exportT1MedianSize);
                                                                                if(exportT1 > 1 && exportT2 > 1) addFloatToMedianArr(exportMedianArr, &exportMedianSize, BIG_MEDIAN_SIZE_MAX, exportT1 + exportT2);
                                                                            break;

                                                                            case 0x02:
                                                                                addFloatToMedianArr(exportT2MedianArr, &exportT2MedianSize, BIG_MEDIAN_SIZE_MAX, meterReadRaw / 10000.0f);
                                                                                exportT2 = medianFloat(exportT2MedianArr, exportT2MedianSize);
                                                                                if(exportT1 > 1 && exportT2 > 1) addFloatToMedianArr(exportMedianArr, &exportMedianSize, BIG_MEDIAN_SIZE_MAX, exportT1 + exportT2);
                                                                            break;
                                                                        }
                                                                    break;
                                                                }

                                                                break;
                                                            }
                                                        }

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
            }
            //printf("\n\n");

            ESP_LOGI("Power", "Overall: %ld  L1: %ld  L2: %ld  L3: %ld", currentPower, currentPowerL1, currentPowerL2, currentPowerL3);
            ESP_LOGI("Meter readings", "Import: %.3f  T1: %.3f  T2: %.3f  Export: %.3f  T1: %.3f  T2: %.3f", importOverall, importT1, importT2, exportOverall, exportT1, exportT2);

            ESP_ERROR_CHECK(uart_flush_input(irUART));
        }
    }
}


// From here on everything related to the web server
// Handler for GET "/"
esp_err_t GET_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
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

// Handler for GET "/styles.css"
esp_err_t GET_handler_styles(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (char*)stylesCssFile, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/styles.css\"");
    return ESP_OK;
}

// URI handler structure for GET "/styles.css"
httpd_uri_t uri_GET_styles = {
    .uri      = "/styles.css",
    .method   = HTTP_GET,
    .handler  = GET_handler_styles,
    .user_ctx = NULL
};

// Handler for GET "/updater.js"
esp_err_t GET_handler_updater(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (char*)updaterJsFile, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/updater.js\"");
    return ESP_OK;
}

// URI handler structure for GET "/updater.js"
httpd_uri_t uri_GET_updater = {
    .uri      = "/updater.js",
    .method   = HTTP_GET,
    .handler  = GET_handler_updater,
    .user_ctx = NULL
};

// Handler for GET "api/values"
esp_err_t GET_handler_api_values(httpd_req_t *req) {
    char resp[1024];
    sprintf(resp, "{\"power\":{\"total\":%ld, \"l1\":%ld, \"l2\":%ld, \"l3\":%ld}, \"meter\":{\"import\":{\"total\":%.2f, \"t1\":%.2f, \"t2\":%.2f}, \"export\":{\"total\":%.2f, \"t1\":%.2f, \"t2\":%.2f}}, \"up\":\"%s\"}"
        , currentPower, currentPowerL1, currentPowerL2, currentPowerL3, importOverall, importT1, importT2, exportOverall, exportT1, exportT2, upSinceStr);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/api/values\"");
    return ESP_OK;
}

// URI handler structure for GET "/api/values"
httpd_uri_t uri_GET_api_values = {
    .uri      = "/api/values",
    .method   = HTTP_GET,
    .handler  = GET_handler_api_values,
    .user_ctx = NULL
};

// Handler for GET "/favicon.svg"
esp_err_t GET_handler_favicon(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/svg+xml");
    httpd_resp_send(req, (char*)faviconSVG_start, faviconSVG_end - faviconSVG_start);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/favicon.svg\"");
    return ESP_OK;
}

// URI handler structure for GET "/favicon.svg"
httpd_uri_t uri_GET_favicon = {
    .uri      = "/favicon.svg",
    .method   = HTTP_GET,
    .handler  = GET_handler_favicon,
    .user_ctx = NULL
};

// Handler for GET "/settings"
esp_err_t GET_handler_settings(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (char*)settingsHtmlFile, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/settings\"");
    return ESP_OK;
}

// URI handler structure for GET "/settings"
httpd_uri_t uri_GET_settings = {
    .uri      = "/settings",
    .method   = HTTP_GET,
    .handler  = GET_handler_settings,
    .user_ctx = NULL
};

// Handler for PUT "/settings"
esp_err_t PUT_handler_settings(httpd_req_t *req) {
    // Parse JSON
    char content[256] = {};
    size_t receivedSize = req->content_len < 256 ? req->content_len : 256;
    if(httpd_req_recv(req, content, receivedSize) <= 0) {
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_send(req, "", 0);
        return ESP_FAIL;
    }

    cJSON* contentJson = cJSON_Parse(content);
    if(contentJson == NULL) {
        httpd_resp_set_status(req, "406 Not Acceptable");
        httpd_resp_send(req, "", 0);
        return ESP_FAIL;
    }

    const cJSON* currency = NULL;
    cJSON* cost = NULL;
    
    currency = cJSON_GetObjectItemCaseSensitive(contentJson, "unit");
    if(cJSON_IsString(currency) && currency->valuestring != NULL) {
        sprintf(settings.currency, "%s", currency->valuestring);
    }

    cost = cJSON_GetObjectItemCaseSensitive(contentJson, "import");
    if(cJSON_IsNumber(cost)) {
        settings.importCost = (float)cost->valuedouble;
    }

    cost = cJSON_GetObjectItemCaseSensitive(contentJson, "importT1");
    if(cJSON_IsNumber(cost)) {
        settings.importCostT1 = (float)cost->valuedouble;
    }

    cost = cJSON_GetObjectItemCaseSensitive(contentJson, "importT2");
    if(cJSON_IsNumber(cost)) {
        settings.importCostT2 = (float)cost->valuedouble;
    }

    cost = cJSON_GetObjectItemCaseSensitive(contentJson, "export");
    if(cJSON_IsNumber(cost)) {
        settings.exportCost = (float)cost->valuedouble;
    }

    cost = cJSON_GetObjectItemCaseSensitive(contentJson, "exportT1");
    if(cJSON_IsNumber(cost)) {
        settings.exportCostT1 = (float)cost->valuedouble;
    }

    cost = cJSON_GetObjectItemCaseSensitive(contentJson, "exportT2");
    if(cJSON_IsNumber(cost)) {
        settings.exportCostT2 = (float)cost->valuedouble;
    }

    // Write settings to NVS
    nvs_handle_t settingsHandle;
    esp_err_t ret = nvs_open("siteSettings", NVS_READWRITE, &settingsHandle);
    if(ret != ESP_OK) {
        httpd_resp_set_status(req, HTTPD_500);
        httpd_resp_send(req, "", 0);
        return ESP_FAIL;
    }

    nvs_set_u32(settingsHandle, "importCost", (uint32_t)(settings.importCost * 100.0f));
    nvs_set_u32(settingsHandle, "importCostT1", (uint32_t)(settings.importCostT1 * 100.0f));
    nvs_set_u32(settingsHandle, "importCostT2", (uint32_t)(settings.importCostT2 * 100.0f));
    nvs_set_u32(settingsHandle, "exportCost", (uint32_t)(settings.exportCost * 100.0f));
    nvs_set_u32(settingsHandle, "exportCostT1", (uint32_t)(settings.exportCostT1 * 100.0f));
    nvs_set_u32(settingsHandle, "exportCostT2", (uint32_t)(settings.exportCostT2 * 100.0f));
    nvs_set_str(settingsHandle, "currency", settings.currency);

    nvs_commit(settingsHandle);
    nvs_close(settingsHandle);

    httpd_resp_set_status(req, HTTPD_204);
    httpd_resp_send(req, "", 0);
    ESP_LOGI("HTTP PUT", "Sent PUT response for \"/settings\"");
    return ESP_OK;
}

// URI handler structure for PUT "/settings"
httpd_uri_t uri_PUT_settings = {
    .uri      = "/settings",
    .method   = HTTP_PUT,
    .handler  = PUT_handler_settings,
    .user_ctx = NULL
};

// Handler for GET "/settings/settings.js"
esp_err_t GET_handler_settingsJs(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/javascript");
    httpd_resp_send(req, (char*)settingsJsFile, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/settings/settings.js\"");
    return ESP_OK;
}

// URI handler structure for GET "/settings/settings.js"
httpd_uri_t uri_GET_settingsJs = {
    .uri      = "/settings/settings.js",
    .method   = HTTP_GET,
    .handler  = GET_handler_settingsJs,
    .user_ctx = NULL
};

// Handler for GET "api/settings"
esp_err_t GET_handler_api_settings(httpd_req_t *req) {
    char resp[1024];
    sprintf(resp, "{\"costs\":{\"currency\":\"%s\", \"import\":{\"overall\":%.2f, \"T1\":%.2f, \"T2\":%.2f}, \"export\":{\"overall\":%.2f, \"T1\":%.2f, \"T2\":%.2f}}}"
        , settings.currency, settings.importCost, settings.importCostT1, settings.importCostT2, settings.exportCost, settings.exportCostT1, settings.exportCostT2);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI("HTTP GET", "Sent GET response for \"/api/settings\"");
    return ESP_OK;
}

// URI handler structure for GET "/api/settings"
httpd_uri_t uri_GET_api_settings = {
    .uri      = "/api/settings",
    .method   = HTTP_GET,
    .handler  = GET_handler_api_settings,
    .user_ctx = NULL
};

// Final server set up and start
httpd_handle_t start_webserver() {
    // Generate default config
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;

    // Empty handle to esp_http_server
    httpd_handle_t server = NULL;

    // Start the httpd server
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_GET);
        httpd_register_uri_handler(server, &uri_GET_api_values);
        httpd_register_uri_handler(server, &uri_GET_favicon);
        httpd_register_uri_handler(server, &uri_GET_styles);
        httpd_register_uri_handler(server, &uri_GET_updater);
        httpd_register_uri_handler(server, &uri_GET_settings);
        httpd_register_uri_handler(server, &uri_GET_settingsJs);
        httpd_register_uri_handler(server, &uri_PUT_settings);
        httpd_register_uri_handler(server, &uri_GET_api_settings);
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
        
        ESP_LOGE("WIFI", "Connection to the AP failed");

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI", "IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retries = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

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
    strcpy((char*)wifi_config.sta.ssid, CONFIG_YOUR_AP_SSID);
	strcpy((char*)wifi_config.sta.password, CONFIG_YOUR_AP_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

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
        ESP_LOGI("WIFI", "Successfully connected to AP with SSID: %s", CONFIG_YOUR_AP_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE("WIFI", "Failed to connect to AP with SSID: %s", CONFIG_YOUR_AP_SSID);
    } else {
        ESP_LOGE("WIFI", "UNEXPECTED EVENT");
    }
}

static void httpServerTask(void* args) {
    esp_netif_init();
    esp_event_loop_create_default();

    ESP_LOGI("HTTP server", "Preparing SNTP time sync via DHCP");
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(1, ESP_SNTP_SERVER_LIST("pool.ntp.org"));
    sntp_config.start = false;
    sntp_config.server_from_dhcp = true;
    sntp_config.renew_servers_after_new_IP = true;
    sntp_config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
    sntp_config.index_of_first_server = 1;
    ESP_ERROR_CHECK(esp_netif_sntp_init(&sntp_config));

    // Connect to WiFi
    wifi_init_sta();

    ESP_LOGI("HTTP server", "Staring SNTP time sync...");
    esp_netif_sntp_start();

    uint8_t retry = 0;
    const uint8_t retry_count = 15;
    while (esp_netif_sntp_sync_wait(3000 / portTICK_PERIOD_MS) != ESP_OK && ++retry <= retry_count) {
        ESP_LOGI("HTTP server", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
    }

    esp_netif_sntp_deinit();

    serverHandle = start_webserver();

    if(serverHandle == NULL) {
        ESP_LOGE("HTTP server", "Failed to start server! Restarting...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    }

    ESP_LOGI("HTTP server", "Successfully started server!");
    time(&upSince);
    setenv("TZ", CONFIG_YOUR_TIME_ZONE, 1);
    tzset();
    localtime_r(&upSince, &timeinfo);
    strftime(upSinceStr, sizeof(upSinceStr), "%c", &timeinfo);

    ESP_LOGI("HTTP server", "Up since: %s", upSinceStr);

    while(1) {
        vTaskDelay(1);
    }
}


void app_main(void) {
    if(strcmp(CONFIG_YOUR_AP_SSID, "MyExampleAccessPoint1234") == 0) ESP_LOGW("CONFIG", "Your AP SSID matches the default, make sure your config is correct (see README.md).");
    if(strcmp(CONFIG_YOUR_AP_PASSWORD, "MySuperSecureExamplePassword1234") == 0) ESP_LOGW("CONFIG", "Your AP password matches the default, make sure your config is correct (see README.md).");
    if(strcmp(CONFIG_YOUR_TIME_ZONE, "UTC0") == 0) ESP_LOGW("CONFIG", "Your timezone matches the default, if you want to use UTC0 there is nothing to worry about, if need a different timezone check your config (see README.md).");

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load settings from NVS
    nvs_handle_t settingsHandle;
    ret = nvs_open("siteSettings", NVS_READONLY, &settingsHandle);
    if(ret != ESP_OK) {
        ESP_LOGW("NVS settings", "Could not load settings from NVS. Not a problem if no settings have been saved yet.");
    } else {
        ESP_LOGI("NVS settings", "Loading settings...");
        uint32_t costsRaw = 0;

        ret = nvs_get_u32(settingsHandle, "importCost", &costsRaw);
        if(ret == ESP_OK) settings.importCost = ((float)costsRaw) / 100.0f;
        ret = nvs_get_u32(settingsHandle, "importCostT1", &costsRaw);
        if(ret == ESP_OK) settings.importCostT1 = ((float)costsRaw) / 100.0f;
        ret = nvs_get_u32(settingsHandle, "importCostT2", &costsRaw);
        if(ret == ESP_OK) settings.importCostT2 = ((float)costsRaw) / 100.0f;
        ret = nvs_get_u32(settingsHandle, "exportCost", &costsRaw);
        if(ret == ESP_OK) settings.exportCost = ((float)costsRaw) / 100.0f;
        ret = nvs_get_u32(settingsHandle, "exportCostT1", &costsRaw);
        if(ret == ESP_OK) settings.exportCostT1 = ((float)costsRaw) / 100.0f;
        ret = nvs_get_u32(settingsHandle, "exportCostT2", &costsRaw);
        if(ret == ESP_OK) settings.exportCostT2 = ((float)costsRaw) / 100.0f;

        size_t currencySize;
        ret = nvs_get_str(settingsHandle, "currency", NULL, &currencySize);
        if(ret == ESP_OK && !(currencySize > (size_t)8)) nvs_get_str(settingsHandle, "currency", settings.currency, &currencySize);

        nvs_close(settingsHandle);
    }

    xTaskCreatePinnedToCore(irReaderTask, "irReaderTask", 20000, NULL, 10, NULL, 1);
    xTaskCreatePinnedToCore(httpServerTask, "httpServerTask", 10000, NULL, 10, NULL, 0);
}


// ### Array handling helpers to calculate median for 1.8.0 and 2.8.0
// shifts everything one index up and then inserts the value at 0
void insertFloatAtFirstIndex(float* arr, uint16_t len, float value) {
    for(int32_t i = len - 2; i >= 0; i--) arr[i+1] = arr[i];

    arr[0] = value;
}

void insertI32AtFirstIndex(int32_t* arr, uint16_t len, int32_t value) {
    for(int32_t i = len - 2; i >= 0; i--) arr[i+1] = arr[i];

    arr[0] = value;
}

// takes an unsorted array and calculates the median over it
float medianFloat(float* arr, uint16_t len) {
    // we need to sort the array to calculate the median, but we leave the input array untouched
    float sortArr[len] = {};
    memcpy(sortArr, arr, len * sizeof(float));
    qsort(sortArr, len, sizeof(float), compareFloat);

    if(len % 2) { // if len is odd, only need to evaluate one element
        return sortArr[(len/2)];
    } else { // if len is even, need to calculate average over middle two elemets
        return (sortArr[(len/2) - 1] + sortArr[(len/2)]) / 2.0f;
    }
}

int32_t medianI32(int32_t* arr, uint16_t len) {
    // we need to sort the array to calculate the median, but we leave the input array untouched
    int32_t sortArr[len] = {};
    memcpy(sortArr, arr, len * sizeof(int32_t));
    qsort(sortArr, len, sizeof(int32_t), compareI32);

    if(len % 2) { // if len is odd, only need to evaluate one element
        return sortArr[(len/2)];
    } else { // if len is even, need to calculate average over middle two elemets
        return (sortArr[(len/2) - 1] + sortArr[(len/2)]) / 2;
    }
}

// used for the sorting in median calculation
int compareFloat(const void* f1, const void* f2) {
    float float1 = *((float*)f1);
    float float2 = *((float*)f2);

    if(float1 > float2) return 1;
    if(float2 > float1) return -1;
    return 0;
}

int compareI32(const void* i1, const void* i2) {
    int32_t int1 = *((int32_t*)i1);
    int32_t int2 = *((int32_t*)i2);

    if(int1 > int2) return 1;
    if(int2 > int1) return -1;
    return 0;
}

void addFloatToMedianArr(float* arr, uint8_t* sizeCounter, uint8_t sizeMax,float value) {
    insertFloatAtFirstIndex(arr, *sizeCounter, value);
    if(*sizeCounter < sizeMax) (*sizeCounter)++;
}

void addI32ToMedianArr(int32_t* arr, uint8_t* sizeCounter, uint8_t sizeMax, int32_t value) {
    insertI32AtFirstIndex(arr, *sizeCounter, value);
    if(*sizeCounter < sizeMax) (*sizeCounter)++;
}