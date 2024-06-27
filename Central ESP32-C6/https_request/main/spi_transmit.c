
#include "spi_transmit.h"

#define GPIO_HANDSHAKE  15  // handshake line   ->      16
#define GPIO_MOSI       21  // mosi line        ->      13
#define GPIO_MISO       20  // miso line        ->      14
#define GPIO_SCLK       19  // sclk line        ->      12
#define GPIO_CS         18  // cs line          ->      15

#define RCV_HOST    SPI2_HOST

spi_slave_transaction_t t;

void my_post_setup_cb(spi_slave_transaction_t *trans)
{
    gpio_set_level(GPIO_HANDSHAKE, 1);
}

void my_post_trans_cb(spi_slave_transaction_t *trans)
{
    gpio_set_level(GPIO_HANDSHAKE, 0);
}

esp_err_t spi_slave_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_MOSI,
        .miso_io_num = GPIO_MISO,
        .sclk_io_num = GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .queue_size = 3,
        .flags = 0,
        .post_setup_cb = my_post_setup_cb,
        .post_trans_cb = my_post_trans_cb
    };

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1 << GPIO_HANDSHAKE)
    };

    gpio_config(&io_conf);
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    return spi_slave_initialize(RCV_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
}

esp_err_t spi_receive(char *sendbuf, char *recvbuf, TickType_t ticks_to_wait)
{
    //WORD_ALIGNED_ATTR char sendbuf[129] = "";
    //WORD_ALIGNED_ATTR char recvbuf[129] = "";
    esp_err_t ret;
    
    memset(&t, 0, sizeof(t));

    memset(recvbuf, 0x00, 129);
    //sprintf(sendbuf, "%s", buf);

    t.length = 128 * 8;
    t.tx_buffer = sendbuf;
    t.rx_buffer = recvbuf;


    ret = spi_slave_transmit(RCV_HOST, &t, ticks_to_wait);
    return ret;


    // if (ret != ESP_OK) 
    // {
    //     return NULL;
    // }

    // return &recvbuf;
}