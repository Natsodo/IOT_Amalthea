
#include "time_sync.h"


static const char *TIME_SYNC = "TIME_SYNC";

//Initialize the time_sync from "pool.ntp.org" 
esp_err_t time_sync_sntp_init(void)
{
    ESP_LOGI(TIME_SYNC, "Initializing SNTP");
    esp_sntp_config_t config = {
        .num_of_servers = (2), 
        .wait_for_sync = 1, 
        .start = 1,
        .servers = { "time.windows.com", "pool.ntp.org" },
    };                 
    
    return esp_netif_sntp_init(&config);
}

int is_wintertime(struct tm tm) {
    struct tm last_sunday_oct = {0};
    struct tm last_sunday_mar = {0};
    last_sunday_oct.tm_year = tm.tm_year;
    last_sunday_mar.tm_year = tm.tm_year;
    last_sunday_oct.tm_mon = 9;
    last_sunday_mar.tm_mon = 2;
    last_sunday_oct.tm_mday = 31; 
    last_sunday_mar.tm_mday = 31; 

    while (last_sunday_oct.tm_wday != 0) { 
        last_sunday_oct.tm_mday--;
    }

    while (last_sunday_mar.tm_wday != 0) { 
        last_sunday_mar.tm_mday--;
    }

    if (tm.tm_mon > last_sunday_oct.tm_mon || (tm.tm_mon == last_sunday_oct.tm_mon && tm.tm_mday >= last_sunday_oct.tm_mday)) {
        if (tm.tm_mon < last_sunday_mar.tm_mon || (tm.tm_mon == last_sunday_mar.tm_mon && tm.tm_mday < last_sunday_mar.tm_mday)) {
            return 1; 
        }
    }
    return 0; 
}
//change sntp time to the right timezone
struct tm adjust_timezone() 
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    int hour_offset = 2;

    if (is_wintertime(timeinfo)){
         hour_offset = 1;
    }
    int adjusted_hour = timeinfo.tm_hour + hour_offset;
    if (adjusted_hour > 24) {
        timeinfo.tm_mday += 1;
        adjusted_hour -= 24;
    }
    timeinfo.tm_hour = adjusted_hour;
    return timeinfo;
}

//Get current system time
char* get_current_time_char(void) 
{
    struct tm timeinfo;
    char formatted_time[16];

    timeinfo = adjust_timezone();
    ESP_LOGI(TIME_SYNC, "%s", asctime(&timeinfo));
    strftime(formatted_time, sizeof(formatted_time), "%Y%m%d%H%M%S", &timeinfo); 
    return strdup(formatted_time); 
}

//Synchronize time from sntp
void get_sntp_time(void) 
{  
    int retry = 0;

    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(ONE_SECOND)) != ESP_OK && ++retry < MAX_RETRY) 
    {
        vTaskDelay(ONE_SECOND / portTICK_PERIOD_MS);
        ESP_LOGI(TIME_SYNC, "Waiting for system time to be set... (%d/%d)", retry, MAX_RETRY);
    }

}