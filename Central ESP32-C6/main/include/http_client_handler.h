
#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"

struct header {
    char *key;
    char *value;
};
void init_http_client(void);
int http_post_token_auth(bool refresh);
int http_post_message_multipart(char *post_data, char *bearer_token);
char *get_bearer_token(void);