#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include <wifi_config.h>
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "driver/gpio.h"
#include "waiter.h"
#include "mqttclient.h"
#include "driver/temperature_sensor.h"

static temperature_sensor_handle_t temp_handle = NULL;
static temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 50);
void get_temperature(float * temperature)
{
    // Enable temperature sensor
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
    // Get converted sensor data
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, temperature));
    // Disable the temperature sensor if it is not needed and save the power
    ESP_ERROR_CHECK(temperature_sensor_disable(temp_handle));
}


static const char *TAG = "HTTP_CLIENT";


#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        //case HTTP_EVENT_ON_HEADERS_COMPLETE:
        //    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADERS_COMPLETE");
        //    break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
        default:
            break;
    }
    ESP_LOGI("http", "status code: %d", esp_http_client_get_status_code(evt->client));
    return 0;
}

esp_http_client_config_t config = {
    .url = "http://api.ipify.org/",
    .transport_type = HTTP_TRANSPORT_OVER_TCP,
    .event_handler = _http_event_handler,
    .buffer_size = 50
    
};

void get_ip_address(char* buffer)
{
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_err_t err = esp_http_client_open(client, 0);

    if (err == ESP_OK)
    {
      //char buff[4*3+4] = {0};
      int down = esp_http_client_fetch_headers(client);
      int len = esp_http_client_read(client, buffer, 100);


      printf("Got %s in %d\n", buffer, down);
    }
    esp_http_client_cleanup(client);
    esp_http_client_close(client);
}

void info_task()
{
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_handle));

    char ipAddress[4*3+4 + 1] = {0};
    get_ip_address(ipAddress);
    float temperature;
    get_temperature(& temperature);
    send_info(temperature, ipAddress);
    while(true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000 * 60 * 60 * 60));
    }
}


void setup_info()
{
    xTaskCreate(info_task, "info_task", 2048, NULL, 10, NULL);
}
