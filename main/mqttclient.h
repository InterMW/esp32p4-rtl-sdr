#ifndef MQTT_CLIENT_T
#define MQTT_CLIENT_T

void send_mqtt(char* message);
void mqtt_task();
void init_mqtt();
extern struct mode_s_msg;
void enqueue_message(struct mode_s_msg* message);

int mqtt_is_ready();
int is_callback_verified();

#endif
