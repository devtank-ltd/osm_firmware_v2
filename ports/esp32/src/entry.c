#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_task_wdt.h>

extern int osm_main(void);

static void _osm_main_task(void *pvParameters)
{
   osm_main();
}


void app_main(void)
{
    TaskHandle_t handle = {0};
    xTaskCreate(_osm_main_task, "osm_main_task", 1024*16, NULL, 12, &handle);
    esp_task_wdt_add(handle);
    for(;;) vTaskDelay(INT_MAX);
}
