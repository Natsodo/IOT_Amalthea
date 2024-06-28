#include "http_client_handler.h"


#define MESSAGE_URL "https://mingle-ionapi.eu1.inforcloudsuite.com/ICSGDENA029_TST/IONSERVICES/api/ion/messaging/service/v3/message/multipart"
#define AUTH_URL "https://mingle-sso.eu1.inforcloudsuite.com:443/ICSGDENA029_TST/as/token.oauth2"
#define LOCAL_URL "http://192.168.14.98:5000/sent_data" 

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

#define client_id "ICSGDENA029_TST~dq_6K20nr5zRXXCtnhtfHds7W7vFjJvjrU2dX_qxtyQ"
#define client_secret "Sr7_lJ0SvPYbADmRUIxlmpPS21KYXN1mSD53li_LOZypKnRPiHqALhjGfJBLl8kXHaB0b3nIcOjYb3ioYtOjPQ"
#define username "ICSGDENA029_TST#MA1qTD0rX1d4vz7CwMMuNiNnSGI_2zQEkpTHJLeTNCa7GqVoPIOJZxKtO8KS-gHrUcspCC2LUoseKjcue_OouA"
#define password "HO8IZjnAjLYNZUG8GZLMLvYtxyfNIqsFFYbNiNj3CLD6ae6G1fO5ty3CGrfBcUP3K6ROZ5OKQoBeNEUNU2gDrw"

static const char *HTTP_CLIENT = "HTTP_CLIENT";
//URL
//static char *ionapi = "https://mingle-ionapi.eu1.inforcloudsuite.com";
//static char *sso = "mingle-sso.eu1.inforcloudsuite.com";
//static char *local = "http://192.168.14.98:5000";

//HEADERS
//static struct header json = {"Content-Type", "application/json"};
static struct header form = {"Content-Type", "application/x-www-form-urlencoded"};
static struct header multi = {"Content-Type", "multipart/form-data; boundary=--------------------------1"};

//static struct header accept_header = {"Accept", "*/*"};
static struct header encoding = {"Accept-Encoding", "gzip, deflate, br"};
static struct header keep_alive = {"Connection", "keep-alive"};

static struct header fromLogicalId = {"X-Infor-ION-fromLogicalId", "lid://infor.ims.iot_ims_datalake"};
static struct header documentName = {"X-Infor-ION-documentName", "IOTSensorData"};
static struct header ION_encoding = {"X-Infor-ION-encoding", "NONE"};
struct header bearer = {"Authorization", ""};

//CERTIFICATES
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

//TOKENS
char *refresh_token;
char *access_token;
char *errors;

//char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

esp_http_client_handle_t post_client; 

static char *response_buffer = NULL;
//char *local_response_buffer;
static int response_buffer_len = 0;

char *get_bearer_token(void)
{
    return access_token;
}

esp_err_t client_event_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_HEADER:
        //ESP_LOGI(HTTP_CLIENT, "%d", evt->data_len);
        break;

    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(HTTP_CLIENT, "HTTP_EVENT_ON_DATA with length %d", evt->data_len);
        // if (esp_http_client_is_chunked_response(evt->client))
        // {
            if (response_buffer == NULL) {
                response_buffer = (char *)malloc(evt->data_len);
                response_buffer_len = evt->data_len;
            } else {
                response_buffer = (char *)realloc(response_buffer, response_buffer_len + evt->data_len);
                response_buffer_len += evt->data_len;
            }
            if (response_buffer == NULL) {
                ESP_LOGE(HTTP_CLIENT, "Failed to allocate memory for response buffer");
                return ESP_FAIL;
            }

            memcpy(response_buffer + response_buffer_len - evt->data_len, evt->data, evt->data_len);
        // } else {
        //     response_buffer = (char *)malloc(evt->data_len);
        //     if (response_buffer == NULL) {
        //         ESP_LOGE(HTTP_CLIENT, "Failed to allocate memory for response buffer");
        //         return ESP_FAIL;
        //     }
        //     memcpy(response_buffer, evt->data, evt->data_len);
        //     response_buffer_len = evt->data_len;

        // }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(HTTP_CLIENT, "HTTP_EVENT_ON_FINISH");
        if (esp_http_client_is_chunked_response(evt->client)){
            if (evt->data_len == 0) {
                cJSON *cjson = cJSON_Parse(response_buffer);
                if (cjson == NULL) {
                    ESP_LOGE(HTTP_CLIENT, "Failed to parse JSON");

                } else {
                    cJSON *access_token_object = cJSON_GetObjectItem(cjson, "access_token");
                    cJSON *refresh_token_object = cJSON_GetObjectItem(cjson, "refresh_token");
                    access_token =  strdup(access_token_object->valuestring);
                    refresh_token = strdup(refresh_token_object->valuestring);
                    cJSON_Delete(cjson);
                }

            }
        }
        else {
            printf("Response: %s\n", response_buffer);

            cJSON *cjson = cJSON_Parse(response_buffer); 
            if (cjson == NULL) {
                ESP_LOGE(HTTP_CLIENT, "Failed to parse JSON");
            } else {
                cJSON *errors_object = cJSON_GetObjectItemCaseSensitive(cjson, "errors");
                cJSON *error;
                cJSON_ArrayForEach(error, errors_object) {
                    printf("Response: %s\n", error->valuestring);
                }
            }

            cJSON_Delete(cjson);
        }
        free(response_buffer);
        response_buffer = NULL;
        response_buffer_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(HTTP_CLIENT, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(HTTP_CLIENT, "Last esp error code: 0x%x", err);
            ESP_LOGI(HTTP_CLIENT, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

void init_http_client(void)
{
    esp_http_client_config_t post_config = {
        .host = "https://mingle-ionapi.eu1.inforcloudsuite.com",
        .path = "/",
        .timeout_ms = 10000,
        .cert_pem = (char *)server_root_cert_pem_start,
        .cert_len = sizeof((char *)server_root_cert_pem_start),
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        //.user_data = local_response_buffer,        // Pass address of local buffer to get response
        .disable_auto_redirect = true,
        .buffer_size_tx = 2048,
        .buffer_size = 128,
        .event_handler = client_event_handler,
        .keep_alive_enable = true,
    };
    post_client = esp_http_client_init(&post_config);
    if (post_client == NULL) {
        ESP_LOGE(HTTP_CLIENT, "Failed to initialise HTTP connection");
        return;
    }
    esp_http_client_set_url(post_client, MESSAGE_URL);
    esp_http_client_set_method(post_client, HTTP_METHOD_POST);
    esp_http_client_set_header(post_client, multi.key, multi.value);
    esp_http_client_set_header(post_client, fromLogicalId.key, fromLogicalId.value);
    esp_http_client_set_header(post_client, documentName.key, documentName.value);
    esp_http_client_set_header(post_client, ION_encoding.key, ION_encoding.value);
    
}

int http_post_token_auth(bool refresh)
{
    char json_buffer[1024];

    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    int htpp_status_code = 0;
    esp_err_t err = ESP_OK;
    int64_t  content_length = 0;

    if(!refresh){
        sprintf(json_buffer, 
        "grant_type=password&client_id=%s&client_secret=%s&username=%s&password=%s",
        client_id, client_secret, username, password);
    } else {
        sprintf(json_buffer, 
        "grant_type=refresh_token&client_id=%s&client_secret=%s&refresh_token=%s",
        client_id, client_secret, refresh_token);
    }
    
    esp_http_client_config_t config = {
        .url = AUTH_URL,
        .cert_pem = (char *)server_root_cert_pem_start,
        .cert_len = sizeof((char *)server_root_cert_pem_start),
        .timeout_ms = 10000,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = client_event_handler,
        //.user_data = local_response_buffer,        // Pass address of local buffer to get response
        .disable_auto_redirect = false,
        .keep_alive_enable = true,
        .keep_alive_count = 10,
        //.buffer_size = sizeof(local_response_buffer)
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_url(client, AUTH_URL);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, form.key, form.value);
    esp_http_client_set_header(client, encoding.key, encoding.value);
    esp_http_client_set_header(client, keep_alive.key, keep_alive.value);

    esp_http_client_set_post_field(client, json_buffer, strlen(json_buffer));

    err = esp_http_client_perform(client);

    htpp_status_code = esp_http_client_get_status_code(client);
    content_length = esp_http_client_get_content_length(client);

    if (err == ESP_OK) {
        ESP_LOGI(HTTP_CLIENT, "HTTP POST Status = %d, content_length = %"PRId64, htpp_status_code, content_length);
    } else {
        ESP_LOGI(HTTP_CLIENT, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    printf("%s", local_response_buffer);
    esp_http_client_cleanup(client);
    return htpp_status_code;
}

int http_post_message_multipart(char *post_data, char *bearer_token)
{
    
    int htpp_status_code = 0;
    esp_err_t err = ESP_OK;
    int64_t  content_length = 0;
    char *buffer = malloc(1216 + 1);

    sprintf(buffer, "Bearer %s", bearer_token);
    bearer.value = buffer;
    
    esp_http_client_delete_header(post_client, bearer.key);
    esp_http_client_set_header(post_client, bearer.key, bearer.value);
    esp_http_client_set_post_field(post_client, post_data, strlen(post_data));

    err = esp_http_client_perform(post_client);
    htpp_status_code = esp_http_client_get_status_code(post_client);
    content_length = esp_http_client_get_content_length(post_client);
    if (err == ESP_OK) 
    {
        ESP_LOGI(HTTP_CLIENT, "HTTP POST Status = %d, content_length = %"PRId64, htpp_status_code, content_length);

    } 
    else 
    {
        ESP_LOGE(HTTP_CLIENT, "HTTP POST Status = %d, content_length = %"PRId64, htpp_status_code, content_length);
        ESP_LOGE(HTTP_CLIENT, "HTTP POST request failed: %s", esp_err_to_name(err));
        printf("Error %s\n", errors);
    }
    // esp_http_client_cleanup(client);
    free(buffer);
    return htpp_status_code;
}
