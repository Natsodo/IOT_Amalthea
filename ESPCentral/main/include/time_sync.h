#include <time.h>
#include <sys/time.h>
#include "esp_err.h"
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "stdint.h"
#include "esp_netif_sntp.h"

#define MAX_RETRY 20
#define ONE_SECOND 1000
#define TIME_TASK_STACK_SIZE 2048 
#define TIME_TASK_PRIORITY 1  


esp_err_t time_sync_sntp_init(void);
char* get_current_time_char(void);
void get_sntp_time(void);


