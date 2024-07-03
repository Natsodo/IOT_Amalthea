#ifndef SPI_COMMUNICATION_H
#define SPI_COMMUNICATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define HANDSHAKE DT_NODELABEL(handshake)
#define SPI DT_NODELABEL(spi0)


#define CONFIG_SPI_CS_CTRL_GPIO_PIN 15

#define SPIOP	SPI_WORD_SET(8) | SPI_OP_MODE_MASTER


void spi_ctrl_transmit(uint8_t *data, uint8_t *recvbuf);
void spi_data(int16_t temperature, uint16_t humidity, uint32_t node_id, uint8_t *camu, uint8_t *timestamp, uint8_t *recvbuf);
void spi_sync(uint8_t *sendbuf, uint8_t *recvbuf);
int spi_init(void);

#ifdef __cplusplus
}
#endif


#endif // SPI_COMMUNICATION_H