/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const uint BTN_1_OLED = 28;
const uint BTN_2_OLED = 26;
const uint BTN_3_OLED = 27;

const uint LED_1_OLED = 20;
const uint LED_2_OLED = 21;
const uint LED_3_OLED = 22;

const uint TRIGGER_PIN = 19;
const uint ECHO_PIN = 13;



QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;
SemaphoreHandle_t xSemaphoreTrigger;


void pin_callback(uint gpio, uint32_t events) {
    uint64_t time = to_us_since_boot(get_absolute_time());
    xQueueSendFromISR(xQueueTime, &time, 0);
}

void trigger_task(){
    while(true){
        gpio_put(TRIGGER_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(TRIGGER_PIN, 0);
        xSemaphoreGive(xSemaphoreTrigger);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

void echo_task(){
    uint64_t time;
    double distance;
    uint64_t t_rise = 0;
    uint64_t t_fall = 0;
    while(true){
        if(xQueueReceive(xQueueTime, &time, pdMS_TO_TICKS(100))){
            if (t_rise == 0){
                t_rise = time;
            } else {
                t_fall = time;
            }
        }

        if (t_fall > 0) {
            distance = (t_fall - t_rise) / 58;
            xQueueSend(xQueueDistance, &distance, pdMS_TO_TICKS(100));
                
            t_rise = 0;
            t_fall = 0;
        }
    }
}

void oled_task(){
    ssd1306_init();
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);
    double distance;
    char distance_str[50];
    while(true){
        if(xSemaphoreTake(xSemaphoreTrigger, pdMS_TO_TICKS(100)) == pdTRUE){
            if(xQueueReceive(xQueueDistance, &distance, pdMS_TO_TICKS(10))){
                sprintf(distance_str, "%.2f", distance);
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Distancia: ");
                gfx_draw_string(&disp, 0, 20, 1, "cm");
                gfx_draw_string(&disp, 0, 10, 1, distance_str);
                gfx_draw_line(&disp, 0, 30, distance, 30);
                gfx_show(&disp);

            }
            else{
            gfx_clear_buffer(&disp);
            gfx_draw_string(&disp, 0, 0, 1, "Sem sinal");
            gfx_show(&disp);
        }
        }


    }

}
int main() {
    stdio_init_all();

    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &pin_callback);

    xTaskCreate(trigger_task, "Trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED", 4095, NULL, 1, NULL);

    xQueueTime = xQueueCreate(10, sizeof(uint64_t));
    xQueueDistance = xQueueCreate(10, sizeof(double));
    xSemaphoreTrigger = xSemaphoreCreateBinary();



    vTaskStartScheduler();

    while (true)
        ;
    

}
