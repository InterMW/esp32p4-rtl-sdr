#ifndef WAITER_H
#define WAITER_H

#define LIMITEDWAIT(max_count,delay, condition, tag, message) { int i = 0; while(i++< max_count && !condition){ESP_LOGI(TAG, "waiting for mqtt");vTaskDelay(pdMS_TO_TICKS(500));} if(!condition){esp_restart();}} 
        ;    

#endif
