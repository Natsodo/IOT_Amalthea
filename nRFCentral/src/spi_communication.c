#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/mutex.h>

#include  "env_client.h"
#include "spi_communication.h"

#define SLEEP_TIME_MS   1000

#if !DT_NODE_EXISTS(HANDSHAKE)
#error "Overlay for handshake node not properly defined."
#endif



static struct gpio_dt_spec handshake = GPIO_DT_SPEC_GET_OR(HANDSHAKE, gpios, {0});
// static const struct gpio_dt_spec cs = GPIO_DT_SPEC_GET(CS, gpios);

struct spi_dt_spec spispec = SPI_DT_SPEC_GET(DT_NODELABEL(esp32), SPIOP, 10);

int n = 0;

// typedef struct spi_sensor_data {
//     int id;
//     char *sens;
//     int temp; 
//     int hum;
//     char *camu;
//     char *timestamp;
// } spi_sensor_data;


void spi_ctrl_transmit(uint8_t *sendbuf, uint8_t *recvbuf)
{
	int err;
    struct spi_buf tx_buf = {
        .buf = sendbuf,
        .len = 129
    }; 

    struct spi_buf rx_buf = {
        .buf = recvbuf,
        .len = 129
    };

    struct spi_buf_set tx_bufs = {
        .buffers = &tx_buf,
        .count = 1
    };

    struct spi_buf_set rx_bufs = {
        .buffers = &rx_buf,
        .count = 1
    };
    printk("Sending: %s\n", sendbuf); 
	err = spi_transceive(spispec.bus, &spispec.config, &tx_bufs, &rx_bufs);
	if (err < 0) {
		printk("spi_ctrl_transmit: Error on %d\n", err);
	}
    printk("Received: %s\n", recvbuf);
}


void spi_data(int16_t temperature, uint16_t humidity, uint32_t node_id, uint8_t *camu, uint8_t *timestamp, uint8_t *recvbuf)
{
    n++;
    uint8_t sendbuf[129];
    snprintf((char *)sendbuf, sizeof(sendbuf), 
        "\"ID\":\"%d\",\"SENS\":\"%X\",\"CAMU\":\"%s\",\"TEMP\":%d,\"HUM\":%d,\"TIMESTAMP\":\"%s\"",
       n, node_id, camu , temperature, humidity, timestamp);
    //memcpy(recvbuf, sendbuf, strlen((char *)sendbuf) + 1);
    while(1){
        if (gpio_pin_get_dt(&handshake)==0) {
            spi_ctrl_transmit(sendbuf, recvbuf);
            break;
        }
    }
}

void spi_sync(uint8_t *sendbuf, uint8_t *recvbuf)
{
    //uint8_t sendbuf[129] =  "SYNC";
    //memcpy(recvbuf, sendbuf, strlen((char *)sendbuf) + 1);
    while(1){
        if (gpio_pin_get_dt(&handshake)==0) {
            spi_ctrl_transmit(sendbuf, recvbuf);
            break;
        }
    }
}

int spi_init(void)
{
    int ret;
    struct spi_cs_control spi_cs = { 
        .gpio = GPIO_DT_SPEC_GET(SPI, cs_gpios),
        .delay = 1,
    };


    printk("%s", spi_cs.gpio.port->name);
    
    spispec.config.cs = spi_cs;
    spispec.config.slave = 0;
    spispec.config.frequency = 5000000;

    if (!gpio_is_ready_dt(&handshake)) {
        return 1;
    }

    ret = gpio_pin_configure_dt(&handshake, GPIO_INPUT);
    if (ret < 0) 
    {
        printk("gpio_pin_configure_dt %d Error on %s", ret, handshake.port->name);
        return 2;
    }

    ret = spi_is_ready_dt(&spispec);
    if (ret < 0) 
    {
        printk("gpio_add_callback %d Error on %s", ret, handshake.port->name);
        return 5;
    }
    return 0;
}
