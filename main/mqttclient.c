#include "mqtt_client.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mode-s.h"
#include "secret.h"
#include "esp_ota_ops.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// #define CONFIG_BROKER_URL "mqtt://10.0.0.5:1883"
#define PLANEFRAME_TOPIC "fromdevice/plane"
#define KEEPALIVE_TOPIC "fromdevice/alive"

static const char *TAG = "mqtt5_example";

static esp_mqtt_client_handle_t client;


static volatile int subscription_ready;
static int mqtt_ready;
static volatile int callback_verified;
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // msg_id = esp_mqtt_client_publish(client,PLANEFRAME_TOPIC, "helloooo",0,0,false);
        mqtt_ready = 1;
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d, return code=0x%02x ", event->msg_id, (uint8_t)*event->data);
        subscription_ready = 1;
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        char* data = malloc(event->data_len);
        sprintf(data,"%.*s",event->data_len, event->data);
        if (strcmp("ota_good",data)== 0)
        {
            ESP_LOGW(TAG, "verified");
            callback_verified = 1;
        }
        if (strcmp("restart",data) == 0)
        {
            esp_restart();
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");

        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        break;
    }
}

static QueueHandle_t xFrameQueue = NULL;
struct transfer_message
{
    unsigned char msg[MODE_S_LONG_MSG_BYTES]; // Binary message
    int msgbits;                              // Number of bits in message
};
static uint8_t *mac;
static int mac_len;
static char *mac_string;
static int mac_string_len;
void init_mac()
{
    mac_len = esp_mac_addr_len_get(ESP_MAC_EFUSE_FACTORY);
    mac = malloc(mac_len+1);
printf("%d\n", mac_len);
    esp_efuse_mac_get_default(mac);
    mac_string_len = mac_len * 2 + 1;
    mac_string = malloc(mac_string_len);
    for (int i = 0; i < mac_len; i++)
    {
        printf("%d : %02X\n",i*2,mac[i]);
        sprintf(mac_string + i * 2, "%02X", mac[i]);
    }
    printf("I am attempting to change %d", mac_len*2);
    mac_string[mac_len*2] = '\0';
    printf("%s\n", mac_string);
}
void init_mqtt()
{
    init_mac();
    ESP_LOGE(TAG,"beginning mqtt");
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .session.last_will.topic = "device/died",
        .session.last_will.msg = mac_string,
        .session.last_will.msg_len = 0,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
        .session.keepalive = 60
    };
    xFrameQueue = xQueueCreate(5, sizeof(struct transfer_message));
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    while(!mqtt_ready)
    {
        vTaskDelay(pdMS_TO_TICKS(500));

        ESP_LOGI(TAG, "waiting for mqtt");    
    }

    ESP_LOGI(TAG, "waiting for mqtt %s", mac_string);    
    char* replace_string = "todevice/%s";

    int len = snprintf(NULL,0,replace_string,mac_string);
    char * ota_check_topic = malloc(len*sizeof(char)+1);
    sprintf(ota_check_topic,replace_string,mac_string);
    ESP_LOGI(TAG, "top ic now %s", ota_check_topic);    
    
    int id = esp_mqtt_client_subscribe_single(client, ota_check_topic,1);
    if (id < 0)
    {
        ESP_LOGW(TAG,"couldn't subscribe? %d", id);
        esp_restart();   
    }

    while(!subscription_ready)
{
        vTaskDelay(pdMS_TO_TICKS(500));
         
        ESP_LOGW(TAG,"couldn't heard? %d", subscription_ready);
}
    int msg_id = esp_mqtt_client_publish(client, "device/check", mac_string, 0, 0, 0);
    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

}

int mqtt_is_ready()
{
    return mqtt_ready;
}

int is_callback_verified()
{
    return callback_verified;
}

static int update_verified_value = 0;

void enqueue_message(struct mode_s_msg *message)
{
    if (xFrameQueue)
    {
        struct transfer_message transfer;
        // transfer.msg = message->msg;
        memcpy(&transfer.msg, &message->msg, sizeof(unsigned char) * (message->msgbits / 8));
        transfer.msgbits = message->msgbits;
        // printf("%02X %02X\n", message->msg[0], transfer.msg[0]);
        // printf("We are sending what's at %p\n",message);
        xQueueSend(xFrameQueue,
                   (void *)&transfer,
                   0);
    }
}

void mqtt_keep_alive_task()
{
    while(true)
    {
        printf("Sending keepalive\n");
        int msg_id = esp_mqtt_client_publish(client, KEEPALIVE_TOPIC, mac_string,0, 0, false);
        vTaskDelay(pdMS_TO_TICKS(30*1000));
    }
}
void mqtt_task()
{
    init_mqtt();
        xTaskCreate(&mqtt_keep_alive_task, "keep_alive_task", 2048, NULL, 5, NULL);
    while (true)
    {
        struct transfer_message message;
        xQueueReceive(xFrameQueue, &message, portMAX_DELAY);
        printf("Sending\n");
        char outt[mac_string_len + 112 / 8 + 1];
        // char outt[112/8];
        for (int i = 0; i < mac_string_len; i++)
        {
            // fprintf(stdout, "%02X", mac[i]);
            // sprintf(outt + i * 2, "%c", mac_string[i]);
            outt[i] = mac_string[i];
        }
        outt[mac_string_len-1] = ':';
        for (int i = 0; i < message.msgbits / 8; i++)
        {
            sprintf(outt + mac_len * 2 + 1 + i * 2, "%02X", message.msg[i]);
        }
        printf("%s\n",outt);
        // printf("\n %s recie\n", outt);

        // fprintf(stdout, "%02X was received\n", message);
        // char output[112] = "hallo";

        // fprintf(&output, "%02X", message);
        // printf("got %02X\n", message.msg[0]);
        //
        if (mqtt_ready)
        {
            int msg_id = esp_mqtt_client_publish(client, PLANEFRAME_TOPIC, outt, message.msgbits / 4 + 2 * mac_len + 1, 0, false);
            // ESP_LOGI(TAG, "also sent publish successful, msg_id=%d", msg_id);
            //  int msg_id = esp_mqtt_client_publish(client, PLANEFRAME_TOPIC, outt, mac_len*2+(message.msgbits/8), 0, false);
        }
        
    }
}


void close_connection()
{
    esp_mqtt_client_disconnect(client);
}

void send_mqtt(char *message)
{

    if (mqtt_ready)
    {
        int msg_id = esp_mqtt_client_publish(client, PLANEFRAME_TOPIC, message, 0, 0, false);
        ESP_LOGI(TAG, "also sent publish successful, msg_id=%d", msg_id);
    }
    // ESP_ERROR_CHECK(esp_netif_init());
    // int msg_id = esp_mqtt_client_publish(client,PLANEFRAME_TOPIC, message,0,0,false);
    // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    // X set up port forwarding for mqtt
    // confirm that it works
    // set up publishing
    // set up a/b
    // ???
    // profit
}
