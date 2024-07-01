#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_slave.h"
#include "esp_log.h"

esp_err_t spi_slave_init(void);
esp_err_t spi_receive(char *sendbuf, char *recvbuf, TickType_t ticks_to_wait);