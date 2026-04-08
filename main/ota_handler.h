#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H


void initiate_OTA();
void verify_OTA();
static void ota_example_task(void *pvParameter);

void do_upgrade(void);
#endif
