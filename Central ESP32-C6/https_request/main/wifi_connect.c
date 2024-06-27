
#include <string.h>
#include "wifi_connect.h" 

static const char *WIFI_CLIENT = "WIFI_CLIENT";

#define WIFI_SCAN_RSSI_THRESHOLD -127
#define NETIF_DESC_STA "netif_sta"
#define WIFI_CONN_MAX_RETRY 6
#define WIFI_SSID "H369A89C9BC"
#define WIFI_PASSWORD "F24747DE2F4A"
//#define WIFI_SSID "corp-wifi"
//#define WIFI_PASSWORD "!Rhyt#hm@Inf0r"

// #define WIFI_SSID "WiFiindetrein"
// #define WIFI_PASSWORD "Denkmaar"

static esp_netif_t *s_sta_netif = NULL;
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;
static SemaphoreHandle_t s_semph_get_ip6_addrs = NULL;

const char *ipv6_addr_types_to_str[6] = {
    "ESP_IP6_ADDR_IS_UNKNOWN",
    "ESP_IP6_ADDR_IS_GLOBAL",
    "ESP_IP6_ADDR_IS_LINK_LOCAL",
    "ESP_IP6_ADDR_IS_SITE_LOCAL",
    "ESP_IP6_ADDR_IS_UNIQUE_LOCAL",
    "ESP_IP6_ADDR_IS_IPV4_MAPPED_IPV6"
};

int IP_SET = 0;

SemaphoreHandle_t IPMutex;

wifi_config_t wifi_config = {
    .sta = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
        .scan_method = WIFI_ALL_CHANNEL_SCAN,
        .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
        .threshold.rssi = WIFI_SCAN_RSSI_THRESHOLD,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
    },
};

int wifi_is_connected(void) 
{
    int ret = 0;
    if (xSemaphoreTake(IPMutex, portMAX_DELAY) == pdTRUE){
        ret = IP_SET;
        xSemaphoreGive(IPMutex);
    }
    return ret;

}

bool is_our_netif(const char *prefix, esp_netif_t *netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static void handler_on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (xSemaphoreTake(IPMutex, portMAX_DELAY) == pdTRUE){
        IP_SET = 0;
        xSemaphoreGive(IPMutex);
    }
    ESP_LOGI(WIFI_CLIENT, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    ESP_ERROR_CHECK(err);
}

static void handler_on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    esp_netif_create_ip6_linklocal(esp_netif);
}

static void handler_on_sta_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (!is_our_netif(NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    ESP_LOGI(WIFI_CLIENT, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    if (s_semph_get_ip_addrs) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    } else {
        printf("Got IPv4 event: Interface \"%s\" address: " IPSTR "\n", esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
        ESP_LOGI(WIFI_CLIENT, "- IPv4 address: " IPSTR ",", IP2STR(&event->ip_info.ip));
    }
}

static void handler_on_sta_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    if (!is_our_netif(NETIF_DESC_STA, event->esp_netif)) {
        return;
    }
    esp_ip6_addr_type_t ipv6_type = esp_netif_ip6_get_addr_type(&event->ip6_info.ip);
    ESP_LOGI(WIFI_CLIENT, "Got IPv6 event: Interface \"%s\" address: " IPV6STR ", type: %s", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip), ipv6_addr_types_to_str[ipv6_type]);

    if (ipv6_type == ESP_IP6_ADDR_IS_LINK_LOCAL) {

        if (s_semph_get_ip6_addrs) {
            xSemaphoreGive(s_semph_get_ip6_addrs);
        } else {

            ESP_LOGI(WIFI_CLIENT, "- IPv6 address: " IPV6STR ", type: %s", IPV62STR(event->ip6_info.ip), ipv6_addr_types_to_str[ipv6_type]);
        }
    }
    if (xSemaphoreTake(IPMutex, portMAX_DELAY) == pdTRUE){
        IP_SET = 1;
        xSemaphoreGive(IPMutex);
    }   
}

esp_err_t wifi_sta_do_connect(wifi_config_t wifi_config, bool wait)
{
    if (wait) {
        s_semph_get_ip_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip_addrs == NULL) {
            return ESP_ERR_NO_MEM;
        }
        s_semph_get_ip6_addrs = xSemaphoreCreateBinary();
        if (s_semph_get_ip6_addrs == NULL) {
            vSemaphoreDelete(s_semph_get_ip_addrs);
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &handler_on_wifi_connect, s_sta_netif));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &handler_on_sta_got_ipv6, NULL));

    ESP_LOGI(WIFI_CLIENT, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGI(WIFI_CLIENT, "WiFi connect failed! ret:%x", ret);
        return ret;
    }
    if (wait) {
        ESP_LOGI(WIFI_CLIENT, "Waiting for IP(s)");
        xSemaphoreTake(s_semph_get_ip_addrs, portMAX_DELAY);
        xSemaphoreTake(s_semph_get_ip6_addrs, portMAX_DELAY);
    }
    return ESP_OK;
}

esp_err_t wifi_sta_do_disconnect(void)
{
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &handler_on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &handler_on_wifi_connect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_GOT_IP6, handler_on_sta_got_ipv6));
    if (s_semph_get_ip_addrs) {
        vSemaphoreDelete(s_semph_get_ip_addrs);
    }
    if (s_semph_get_ip6_addrs) {
        vSemaphoreDelete(s_semph_get_ip6_addrs);
    }
    return esp_wifi_disconnect();
}

void wifi_stop(void)
{
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(s_sta_netif));
    esp_netif_destroy(s_sta_netif);
    s_sta_netif = NULL;
}

void wifi_shutdown(void)
{
    wifi_sta_do_disconnect();
    wifi_stop();
}


void wifi_start(void)
{
    IPMutex = xSemaphoreCreateMutex();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.if_desc = NETIF_DESC_STA;
    esp_netif_config.route_prio = 128;
    s_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&wifi_shutdown));
}

// Make Wi-Fi connection
esp_err_t wifi_connect(void)
{
    return wifi_sta_do_connect(wifi_config, true);
}
// #if CONFIG_EXAMPLE_WIFI_SSID_PWD_FROM_STDIN
//     example_configure_stdin_stdout();
//     char buf[sizeof(wifi_config.sta.ssid)+sizeof(wifi_config.sta.password)+2] = {0};
//     ESP_LOGI(TAG, "Please input ssid password:");
//     fgets(buf, sizeof(buf), stdin);
//     int len = strlen(buf);
//     buf[len-1] = '\0'; /* removes '\n' */
//     memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));

//     char *rest = NULL;
//     char *temp = strtok_r(buf, " ", &rest);
//     strncpy((char*)wifi_config.sta.ssid, temp, sizeof(wifi_config.sta.ssid));
//     memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
//     temp = strtok_r(NULL, " ", &rest);
//     if (temp) {
//         strncpy((char*)wifi_config.sta.password, temp, sizeof(wifi_config.sta.password));
//     } else {
//         wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
//     }
// #endif
    
