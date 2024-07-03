/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/*  Private Funtions of protocol example common */

#pragma once

#include "esp_system.h"
#include <string.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_IP6_ADDRS_PER_NETIF (5)

#define CONNECT_PREFERRED_IPV6_TYPE ESP_IP6_ADDR_IS_LINK_LOCAL

extern const char *ipv6_addr_types_to_str[6];

void print_all_netif_ips(const char *prefix);
int wifi_is_connected(void); 
void wifi_start(void);
esp_err_t wifi_connect(void);

#ifdef __cplusplus
}
#endif
