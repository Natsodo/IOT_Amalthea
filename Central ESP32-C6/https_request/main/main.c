
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "esp_tls.h"

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"

#include "cJSON.h"


#include "wifi_connect.h" 
#include "time_sync.h"
#include "spi_transmit.h"
#include "stack.h"
#include "http_client_handler.h"

#define MAX_RETRY 10
#define ONE_MINUTE 60000
#define TEN_SECONDS 10000
#define ONE_SECOND 1000
#define ONE_HOUR 3600000
#define MAX_SYNC 7190000 //HOUR IN MS
#define REFRESH_LIMIT 7200000 //2 HOURS IN MS
#define EXPIRED 0
#define BEARER_TOKEN_SIZE 1217
#define GPIO_HANDSHAKE 15
#define SENSOR_DATA_SIZE 129

//static const char *MAIN_TAG = "MAIN";

//TODO
/*

time sync met nrf

*/

enum spi_states {
    TIME_SYNC = 0,
    DATA = 1,
    SYNC = 2,
};


stack sensor_stack;

SemaphoreHandle_t BearerMutex;
SemaphoreHandle_t TokenMutex;
SemaphoreHandle_t WiFiMutex;
QueueHandle_t dataQueue = NULL;

int token_alive = 0;
int wifi_alive = 0;

typedef struct {
    char *value;
    TickType_t expiryTime;
    TickType_t refreshTime;
} BearerToken_t;

BearerToken_t bearerToken;


void keep_wifi_alive(void *pvParameter)
{
    wifi_start();
    ESP_ERROR_CHECK(wifi_connect());
    while(1){
        if (xSemaphoreTake(WiFiMutex, portMAX_DELAY) == pdTRUE) {
            wifi_alive = wifi_is_connected();
            xSemaphoreGive(WiFiMutex);
        }
        vTaskDelay(ONE_SECOND / portTICK_PERIOD_MS);
    }
}


void receive_bearer_token(void)
{
    int retry = 0;
    while(http_post_token_auth(false) != 200){
        vTaskDelay(ONE_SECOND / portTICK_PERIOD_MS);
        retry++;
        printf("Bearer token retry: %d\n", retry);
        if (retry == MAX_RETRY){
            vTaskDelay(ONE_MINUTE / portTICK_PERIOD_MS);
            retry = 0;
        }
    }
    if (xSemaphoreTake(BearerMutex, portMAX_DELAY) == pdTRUE) {
        bearerToken.value = get_bearer_token();
        bearerToken.expiryTime = xTaskGetTickCount() + pdMS_TO_TICKS(REFRESH_LIMIT);
        bearerToken.refreshTime = xTaskGetTickCount() + pdMS_TO_TICKS(MAX_SYNC);
        xSemaphoreGive(BearerMutex);
    }
    if (xSemaphoreTake(TokenMutex, portMAX_DELAY) == pdTRUE) {
        token_alive = 1;
        xSemaphoreGive(TokenMutex);
    }
}

int refresh_token_handler(void)
{
    UBaseType_t uxHighWaterMark;
    while(xTaskGetTickCount() < bearerToken.expiryTime)
    {
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);

        // Print the minimum free stack space that has been available
        printf("Remaining stack space refresh_token_handler: %u bytes\n", uxHighWaterMark * sizeof(StackType_t));

        if (xSemaphoreTake(TokenMutex, portMAX_DELAY) == pdTRUE) {
            token_alive = 1;
            xSemaphoreGive(TokenMutex);
        }
        vTaskDelay(ONE_MINUTE / portTICK_PERIOD_MS);
        if (xTaskGetTickCount() > bearerToken.refreshTime){
            if (http_post_token_auth(true) == 200){
                if (xSemaphoreTake(BearerMutex, portMAX_DELAY) == pdTRUE) {
                    bearerToken.value = get_bearer_token();
                    bearerToken.expiryTime = xTaskGetTickCount() + pdMS_TO_TICKS(REFRESH_LIMIT);
                    bearerToken.refreshTime = xTaskGetTickCount() + pdMS_TO_TICKS(MAX_SYNC);
                    xSemaphoreGive(BearerMutex);
                }
            } 
            else{
                printf("Failed to get token\n");
            }
        }
        else{
            char *current_time = get_current_time_char();
            printf("Current time: %s\n", current_time);
            printf("Minutes remaining: %d\n", (int)(bearerToken.refreshTime - xTaskGetTickCount())/6000);
        }
    }
    return EXPIRED;
}

void keep_API_alive(void *pvParameter)
{
   
    while(1){
        receive_bearer_token();
        while(refresh_token_handler()){
            //vTaskDelay(ONE_SECOND / portTICK_PERIOD_MS);
            //Token lost
            if (xSemaphoreTake(TokenMutex, portMAX_DELAY) == pdTRUE) {
                token_alive = 0;
                xSemaphoreGive(TokenMutex);
            }
        }
    }
    //never reached
    vTaskDelete(NULL);
}



// int handle_spi_data(char *data)
// {
//     int ret = -1;
//     int id, temp, hum;
//     int http_status_code;
//     char post_data[512];
//     char *timestamp = (char *)calloc(19 , sizeof(char));
//     char *sens = (char *)calloc(11 , sizeof(char));
//     char *camu = (char *)calloc(5 , sizeof(char));
//     sensor_data sensor_data;

//     int num_converted = sscanf(data,
//        "\"ID\":\"%d\",\"SENS\":\"%10[^\"]\",\"CAMU\":\"%4[^\"]\",\"TEMP\":%d,\"HUM\":%d,\"TIMESTAMP\":\"%18[^\"]\"",
//         &id, sens, camu, &temp, &hum, timestamp);
//     sensor_data.temp = temp;
//     sensor_data.hum = hum;
//     sensor_data.sens = sens;
//     sensor_data.camu = camu;
//     sensor_data.timestamp = get_current_time_char();
//     printf("Data: %s\n", data);
//     if (num_converted == 6) { 
//         printf("Formatted data: %s\n", sensor_data_format(sensor_data));
//         if (xSemaphoreTake(WiFiMutex, portMAX_DELAY) == pdTRUE && xSemaphoreTake(TokenMutex, portMAX_DELAY) == pdTRUE && xSemaphoreTake(BearerMutex, portMAX_DELAY) == pdTRUE) {
//             if (wifi_alive && token_alive && bearerToken.value != NULL) {
//             sprintf(post_data, 
//                 "----------------------------1\r\nContent-Disposition: form-data; name=\"MessagePayload\"; filename=\"doc\"\r\nContent-Type: application/octet-stream\r\n\r\n{%s}\r\n----------------------------1--\r\n", 
//                 sensor_data_format(sensor_data));
//                 http_status_code = http_post_message_multipart(post_data, bearerToken.value);
//                 if (http_status_code != 202){
//                     stack_push(&sensor_stack, sensor_data);
//                 }
//             }
//             else {
//                 printf("wifi_alive: %d, token_alive: %d,\n", wifi_alive, token_alive);
//                 stack_push(&sensor_stack, sensor_data);
//             }
//             xSemaphoreGive(TokenMutex);
//             xSemaphoreGive(WiFiMutex);
//             xSemaphoreGive(BearerMutex);
//         }
//         ret = id;
//     } else if (strstr(data, "TIME_SYNC") != NULL) {
//         printf("Received TIME_SYNC.\n");
//         ret = 0;
//     } else {
//         printf("Incorrect data\n");
//     }
//     free(timestamp);
//     free(sens);
//     free(camu);
//     return ret;
// }

int spi_data(char *data)
{
    int id, temp, hum;
    char *timestamp = (char *)calloc(19 , sizeof(char));
    char *sens = (char *)calloc(11 , sizeof(char));
    char *camu = (char *)calloc(5 , sizeof(char));
    int num_converted = sscanf(data,"\"ID\":\"%d\",\"SENS\":\"%10[^\"]\",\"CAMU\":\"%4[^\"]\",\"TEMP\":%d,\"HUM\":%d,\"TIMESTAMP\":\"%18[^\"]\"",
                                    &id, sens, camu, &temp, &hum, timestamp);
    free(timestamp);
    free(sens);
    free(camu);
    if (num_converted == 6) {
         return DATA;
    }
    else{
        if (strstr(data, "TIME_SYNC") != NULL) {
            return TIME_SYNC;
        } else if (strstr(data, "SYNC") != NULL){
            return SYNC;
        }
    }
    return -1;
}

void http_client_task(void *pvParameter)
{
    char receivedData[SENSOR_DATA_SIZE] = "";
    int id, temp, hum;
    int http_status_code;
    char post_data[512];
    char *timestamp = (char *)calloc(19 , sizeof(char));
    char *sens = (char *)calloc(11 , sizeof(char));
    char *camu = (char *)calloc(5 , sizeof(char));
    sensor_data sensor_data;  
    init_http_client(); 
    while(1){
        xQueueReceive(dataQueue, &receivedData, portMAX_DELAY);
        int num_converted = sscanf( receivedData,
                                    "\"ID\":\"%d\",\"SENS\":\"%10[^\"]\",\"CAMU\":\"%4[^\"]\",\"TEMP\":%d,\"HUM\":%d,\"TIMESTAMP\":\"%18[^\"]\"",
                                    &id, sens, camu, &temp, &hum, timestamp);
        sensor_data.temp = temp;
        sensor_data.hum = hum;
        sensor_data.sens = sens;
        sensor_data.camu = camu;
        sensor_data.timestamp = timestamp;   
        if (num_converted == 6) { 
            printf("Formatted data: %s\n", sensor_data_format(sensor_data));
            if (xSemaphoreTake(WiFiMutex, portMAX_DELAY) == pdTRUE && xSemaphoreTake(TokenMutex, portMAX_DELAY) == pdTRUE && xSemaphoreTake(BearerMutex, portMAX_DELAY) == pdTRUE) {
                if (wifi_alive && token_alive && bearerToken.value != NULL) {
                    sprintf(post_data, 
                    "----------------------------1\r\nContent-Disposition: form-data; name=\"MessagePayload\"; filename=\"doc\"\r\nContent-Type: application/octet-stream\r\n\r\n{%s}\r\n----------------------------1--\r\n", 
                    sensor_data_format(sensor_data));
                    http_status_code = http_post_message_multipart(post_data, bearerToken.value);
                    if (http_status_code != 202){
                        stack_push(&sensor_stack, sensor_data);
                        init_http_client();
                    }
                }
                else {
                    printf("wifi_alive: %d, token_alive: %d,\n", wifi_alive, token_alive);
                    stack_push(&sensor_stack, sensor_data);
                }
                xSemaphoreGive(TokenMutex);
                xSemaphoreGive(WiFiMutex);
                xSemaphoreGive(BearerMutex);
            }
            else{
                printf("wifi_alive: %d, token_alive: %d,\n", wifi_alive, token_alive);
            }
        } else {
            printf("Incorrect data\n");
        }
    }
    vTaskDelete(NULL);
}


void spi_data_received(void *pvParameter)
{
    UBaseType_t uxHighWaterMark;
    WORD_ALIGNED_ATTR char sendbuf[SENSOR_DATA_SIZE] = "";
    WORD_ALIGNED_ATTR char recvbuf[SENSOR_DATA_SIZE] = "";

    TickType_t one_hour_sync = xTaskGetTickCount() + pdMS_TO_TICKS(ONE_HOUR);
    esp_err_t err;
    int spi = 0;
    init_http_client();
    while(1){
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        printf("Remaining stack space spi_data_received: %u bytes\n", uxHighWaterMark * sizeof(StackType_t));
        //tijd verstreken
        if (xTaskGetTickCount() > one_hour_sync){
            TickType_t one_hour_sync = xTaskGetTickCount() + pdMS_TO_TICKS(one_hour_sync);
            sprintf(sendbuf, "TIME_SYNC");
        }
        else{
            sprintf(sendbuf, "OK");
        }
        err = spi_receive(sendbuf, recvbuf, ( TickType_t ) 1000 );
        if (err == ESP_OK){
            spi = spi_data(recvbuf);
            switch(spi)
            {
                case TIME_SYNC:
                    sprintf(sendbuf, get_current_time_char());
                    err = spi_receive(sendbuf, recvbuf, ( TickType_t ) 100 );
                    if (err == ESP_OK){
                        if (strstr(recvbuf, "TIME_OK") != NULL){
                            printf("TIME_OK\n");
                        }
                        else{
                            one_hour_sync = 0;
                        }
                    }
                    break;
                case DATA:
                    if (xQueueSendToBack(dataQueue, recvbuf, ( TickType_t ) 10 ) == pdPASS) {
                        sprintf(sendbuf, "DATA_OK");
                    }
                    else{
                        sprintf(sendbuf, "DATA_FAIL");
                    }
                    break;
                case SYNC:
                    printf("SYNC\n");
                    break;
                default:
                    break;
            }
        }
        else if (err==ESP_ERR_TIMEOUT){
                printf("Time out reached\n");
        }
        else {
            printf("No data received\n");
        }
    }
    //never reached
    vTaskDelete(NULL);
}


void randomThread(void *pvParameter)
{
    WORD_ALIGNED_ATTR char sendbuf[SENSOR_DATA_SIZE] = "";
    snprintf((char *)sendbuf, sizeof(sendbuf), 
        "\"ID\":\"%d\",\"SENS\":\"%X\",\"CAMU\":\"%s\",\"TEMP\":%d,\"HUM\":%d,\"TIMESTAMP\":\"%s\"",
        1, 1, "1847", 2150, 5150, "2024");
    while(1){
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        printf( "%s\n", sendbuf);
        if (xQueueSendToBack(dataQueue, sendbuf, ( TickType_t ) 10 ) == pdPASS) {
            printf("DATA_OK");
        }
    }
    vTaskDelete(NULL);
}


void app_main(void)
{
    WiFiMutex = xSemaphoreCreateMutex();
    BearerMutex = xSemaphoreCreateMutex(); 
    TokenMutex = xSemaphoreCreateMutex();
    dataQueue = xQueueCreate(10, SENSOR_DATA_SIZE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(spi_slave_init());
    ESP_ERROR_CHECK(time_sync_sntp_init());
    get_sntp_time();
    stack_init(&sensor_stack, 5);

    xTaskCreate(&keep_wifi_alive, "WIFI", 8196, NULL, 1, NULL);

    vTaskDelay((ONE_SECOND*10) / portTICK_PERIOD_MS);

    xTaskCreate(&keep_API_alive, "API", 8196, NULL, 2, NULL);
    xTaskCreate(&randomThread, "SPI", 8196, NULL, 3, NULL);
    xTaskCreate(&http_client_task, "HTTP", 8196, NULL, 4, NULL);

}
