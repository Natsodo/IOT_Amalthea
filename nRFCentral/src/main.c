#include <stdio.h>
#include <string.h>

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/mutex.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/posix/time.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include <zephyr/random/random.h>

#include "env_client.h"  
#include "spi_communication.h"

#define SLEEP_TIME_MS   1000

#define DEVICE_NAME             "Basisstion1"
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define MAX_CONNECTIONS 20
#define SENSOR_DATA_SIZE 129
#define QUEUE_SIZE 10

#define BT_THREAD_STACK_SIZE 1024
#define SPI_THREAD_STACK_SIZE 1024

#define THREAD_PRIORITY_NOTIFY 5
#define THREAD_PRIORITY_SCAN 5

#define HRS_MEASUREMENT_FLAGS_CONTACT_DETECTED  BIT(1)

//static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
//static const struct gpio_dt_spec led1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);

//static struct bt_conn *default_conn = NULL; 
//static struct bt_conn *default_conn[MAX_CONNECTIONS] = {NULL}; // Array van pointers naar bt_conn

static struct bt_sensorSht_client sht45s_c[MAX_CONNECTIONS];

sys_slist_t conn_list;
int active_connections = 0;


static struct k_mutex conn_mutex;
K_THREAD_STACK_DEFINE(notify_stack_area, BT_THREAD_STACK_SIZE);
struct k_thread notify_thread_data;
k_tid_t notify_tid;

K_THREAD_STACK_DEFINE(bt_thread_stack_area, BT_THREAD_STACK_SIZE);

K_THREAD_STACK_DEFINE(spi_thread_stack_area, SPI_THREAD_STACK_SIZE);

struct spi_data {
	int temperature;
	int humidity;
	int node_id;
	char* counter;
};

K_MSGQ_DEFINE(sensor_data_queue, sizeof(struct spi_data), QUEUE_SIZE, 4);

struct k_thread bt_thread_data;
struct k_thread spi_thread_data;

struct time_data {
	time_t time;
	int64_t uptime;
};

struct conn_node {
    sys_snode_t node;
    struct bt_conn *conn;
};

struct notify_data {
    struct bt_sensorSht_client *sht45s_c;
    struct env_data meas;
    int err;
};



struct time_data current_time = {0, 0};
// bool check_connection_status()
// {
//     if (default_conn != NULL) {
//         return true;
//     } else {
//         return false;
//     }
// }

bool check_connection_status() {
    return active_connections > 0;
}

void add_connection(struct bt_conn *conn) {
    struct conn_node *node = k_malloc(sizeof(struct conn_node));
    if (node) {
        node->conn = bt_conn_ref(conn); //Toevoegen van verwijzing naar de connectie.

        k_mutex_lock(&conn_mutex, K_FOREVER); 
        sys_slist_append(&conn_list, &node->node); 
        active_connections++;
        k_mutex_unlock(&conn_mutex); 

        sys_snode_t *cur; 

        SYS_SLIST_FOR_EACH_NODE(&conn_list, cur) { //for-each loop voor de connectie.
            node = CONTAINER_OF(cur, struct conn_node, node); 
            printk("Connection: [Handle: %p]\n", node->conn);
        }
    
    } else {
        printk("Error: Failed to allocate memory for connection node\n");
    }
}

void remove_connection(struct bt_conn *conn) {
    struct conn_node *node, *tmp;

    k_mutex_lock(&conn_mutex, K_FOREVER);
    SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&conn_list, node, tmp, node) {  
        if (node->conn == conn) {
            sys_slist_find_and_remove(&conn_list, &node->node); //verwijderen van de connectie.
            bt_conn_unref(node->conn); //verwijder de verwijzing naar de connectie.
            k_free(node);
            active_connections--;
            break;
        }
    }
    k_mutex_unlock(&conn_mutex);

    sys_snode_t *cur;

    SYS_SLIST_FOR_EACH_NODE(&conn_list, cur) {
            node = CONTAINER_OF(cur, struct conn_node, node);
            printk("Connection: [Handle: %p]\n", node->conn);
        }
}



char* get_timestamp(int64_t sensor_time, int64_t uptime) {
	char formatted_time[16];
	struct tm *timeinfo;

	int64_t time_difference = sensor_time - current_time.uptime + uptime;
    timeinfo = localtime(&current_time.time);
    timeinfo->tm_sec += (time_difference/1000);
	
    strftime(formatted_time, sizeof(formatted_time), "%Y%m%d%H%M%S", timeinfo); 

    return strdup(formatted_time);
}

void notify_thread(void *p1, void *p2, void *p3) {
    struct notify_data *data = (struct notify_data *)p1;
    char addr[BT_ADDR_LE_STR_LEN];

    if (data == NULL) {
        printk("Error: Invalid measurement data format received\n");
        return;
    }

    // if (!check_connection_status()) {
    //     printk("Error: Connection to Basestation is not active\n");
    //     return;
    // }

    if (data->err) {
        printk("Error during receiving SHT45 sensor Measurement notification, err: %d\n", data->err);
        printk("Received data details:\n");
        printk("\tNode_id: %X\n", data->meas.node_id);
        printk("\tTemperature: %d\n", data->meas.temperature);
        printk("\tHumidity: %d\n", data->meas.humidity);
        printk("\tCounter: %d\n", data->meas.counter);
        return;
    }
	struct spi_data meas;
	meas.temperature = data->meas.temperature;
	meas.humidity = data->meas.humidity;
	meas.node_id = data->meas.node_id;
	//meas.counter = data->meas.counter + data->sht45s_c->time;
	meas.counter = get_timestamp(data->meas.counter, data->sht45s_c->time);
	//data->sht45s_c->time = k_uptime_get();
	//meas.counter = get_timestamp();

	

    printk("SHT45 sensor Measurement notification received:\n\n");
    printk("\tNode_id: %X\n", data->meas.node_id);
    printk("\tTemperature: %d\n", data->meas.temperature);
    printk("\tHumidity: %d\n", data->meas.humidity);
    printk("\tCounter: %d\n", meas.counter);
    printk("\n");

	k_msgq_put(&sensor_data_queue, &meas, K_FOREVER);

    struct conn_node *node;
    SYS_SLIST_FOR_EACH_CONTAINER(&conn_list, node, node) {
        bt_addr_le_to_str(bt_conn_get_dst(node->conn), addr, sizeof(addr));
        printk("See connection: %s\n", addr);
    }

    k_free(data);
}

static void notify_func(struct bt_sensorSht_client *sht45s_c, const struct env_data *meas, int err) {
    struct notify_data *data = k_malloc(sizeof(struct notify_data));
    if (!data) {
        printk("Error: Failed to allocate memory for notify data\n");
        return;
    }

    data->sht45s_c = sht45s_c;
    data->meas = *meas;
    data->err = err;

    k_thread_create(&notify_thread_data, notify_stack_area, K_THREAD_STACK_SIZEOF(notify_stack_area),
                    notify_thread, data, NULL, NULL, THREAD_PRIORITY_NOTIFY, 0, K_NO_WAIT);
}

static void discover_sht45s_completed(struct bt_gatt_dm *dm, void *ctx) {
    int err;
    struct bt_sensorSht_client *sht45_client = ctx;

    printk("The discovery procedure succeeded\n");
    bt_gatt_dm_data_print(dm); // print discovery data

    err = bt_sht45s_client_handles_assign(dm, sht45_client); // assign handles
    if (err) {
        printk("Could not init SHT45S client object (err %d)\n", err);
        return;
    }

//abonnneer op metingen
    err = bt_sht45s_client_measurement_subscribe(sht45_client, notify_func, k_uptime_get()); 
    if (err && err != -EALREADY) {
        printk("Subscribe failed (err %d)\n", err);
    } else {
        printk("[SUBSCRIBED]\n");
    }

    k_sleep(K_SECONDS(2));


     // Send a value to the RTI characteristic
    err = bt_sht45s_client_write_rti(sht45_client, 33); // Send a value to the RTI characteristic.
    if (err) {
        printk("Failed to write to RTI characteristic: %d", err);
        return;
    }

//om geheugen lekken te voorkomen
    err = bt_gatt_dm_data_release(dm);
    if (err) {
        printk("Could not release the discovery data (err %d)\n", err);
    }

//kijk of er nog andere services zijn
    err = bt_gatt_dm_continue(dm, NULL); 
    if (err) {
        printk("Could not continue the discovery procedure (err %d)\n", err);
    }



    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
    }
}

static void discover_sht45s_service_not_found(struct bt_conn *conn, void *ctx)
{
	printk("No more services\n");
}

static void discover_sht45s_error_found(struct bt_conn *conn, int err, void *ctx)
{
	printk("The discovery procedure failed, err %d\n", err);
}

static struct bt_gatt_dm_cb discover_sht45s_cb = {
	.completed = discover_sht45s_completed,
	.service_not_found = discover_sht45s_service_not_found,
	.error_found = discover_sht45s_error_found,
};

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	int err = 0;
	char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn_le_create_param *conn_params;
	int ref = 0;

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	printk("Filters matched. Address: %s connectable: %s\n",
		addr, connectable ? "yes" : "no");

	conn_params = BT_CONN_LE_CREATE_PARAM(
			BT_CONN_LE_OPT_CODED | BT_CONN_LE_OPT_NO_1M,
			BT_GAP_SCAN_FAST_INTERVAL,
			BT_GAP_SCAN_FAST_INTERVAL);

	err = bt_scan_stop();
	if (err) {
		printk("Stop LE scan failed (err %d)\n", err);
	}


	for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (sht45s_c[i].conn == NULL) {
			ref = i;
			break;
		}
	}

	err = bt_conn_le_create(device_info->recv_info->addr, conn_params,
							BT_LE_CONN_PARAM_DEFAULT,
							&sht45s_c[ref].conn);

	// for (int i = 0; i < MAX_CONNECTIONS; i++) {
	// 	printk("i = %d\n", i);
	// 	if (sht45s_c[i].conn == NULL) {
	// 		sht45s_c[i].conn = conn;
	// 		break;
	// 	}
	// }

	if (err) {
		printk("Create conn failed (err %d)\n", err);
	}

	else {
		err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
		if (err) {
			printk("Scanning failed to start (err %d)\n", err);
    	}
		return;
	}
	printk("Connection pending\n");
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL, NULL, NULL);

static void scan_init(void)
{
	int err;

	/* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */
	struct bt_le_scan_param scan_param = {
		.type     = BT_LE_SCAN_TYPE_ACTIVE,
		.interval = BT_GAP_SCAN_FAST_INTERVAL,
		.window   = BT_GAP_SCAN_FAST_WINDOW,
		.options  = BT_LE_SCAN_OPT_CODED | BT_LE_SCAN_OPT_NO_1M
	};

	struct bt_scan_init_param scan_init = {
		.connect_if_match = 0,
		.scan_param = &scan_param,
		.conn_param = NULL
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_ESS);
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);
		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	struct bt_conn_info info;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);

		// for (int i = 0; i < MAX_CONNECTIONS; i++) {
		// 	if (default_conn[i] == conn) {
		// 		bt_conn_unref(default_conn[i]);
		// 		default_conn[i] = NULL;
		// 		break;
		// 	}
		// }
		// return;
	}

	err = bt_conn_get_info(conn, &info);
	if (err) {
		printk("Failed to get connection info (err %d)\n", err);
	} else {
		const struct bt_conn_le_phy_info *phy_info;

		phy_info = info.le.phy;
		printk("Connected: %s, tx_phy %u, rx_phy %u\n",
				addr, phy_info->tx_phy, phy_info->rx_phy);
	}



	for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (sht45s_c[i].conn == NULL) {
			sht45s_c[i].conn = conn;
			bt_sht45s_client_init(&sht45s_c[i]);
			err = bt_gatt_dm_start(conn, BT_UUID_ESS, &discover_sht45s_cb, &sht45s_c[i]);
			if (err) {
				printk("Failed to start discovery (err %d)\n", err);
			}
			break;
		}
	}
	// if (conn == default_conn[ref]) {
	// 	err = bt_gatt_dm_start(conn, BT_UUID_ESS, &discover_sht45s_cb, NULL);
	// 	if (err) {
	// 		printk("Failed to start discovery (err %d)\n", err);
	// 	}
	// }
	
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (sht45s_c[i].conn  == conn) {
			bt_sht45s_client_measurement_unsubscribe(&sht45s_c[i]);
            bt_conn_unref(sht45s_c[i].conn);
            sht45s_c[i].conn = NULL;
            break;
        }
    }
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

int ble_enable(void)
{
	int err;

	printk("Starting Bluetooth Central sht45s\n");

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		sht45s_c[i].conn = NULL;  // Initialize the connection pointer to NULL
		// Initialize other fields as necessary
	}

	// err = bt_sht45s_client_init(&sht45s_c);
	// if (err) {
	// 	printk("SHT45 sensor Service client failed to init (err %d)\n", err);
	// 	return 0;
	// }

	scan_init();

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return 0;
	}   
    printk("Scanning successfully started\n");
    return 1;
}

void convert_time(char *timestamp)
{
	int year, month, day, hour, minute, second;
	struct tm timeinfo;
	if (sscanf(timestamp, "%4d%2d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute, &second) == 6) {// tm_year is years since 1900
		timeinfo.tm_year = year - 1900;
		// tm_mon is 0-based, January is 0
		timeinfo.tm_mon = month - 1;
		timeinfo.tm_mday = day;
		timeinfo.tm_hour = hour;
		timeinfo.tm_min = minute;
		timeinfo.tm_sec = second;
		// Set this to -1 to tell mktime() to determine whether DST is in effect
		timeinfo.tm_isdst = -1;
	}
	current_time.time = mktime(&timeinfo);
	current_time.uptime = k_uptime_get();
	//time(&t);
    //localtime_r(&now, &timeinfo);
	//print the time_t as a string
	
}

void bluetooth_thread(void *unused1, void *unused2, void *unused3)
{
	struct env_data data;
	data.temperature = 1553;
	data.humidity = 7813;
	data.node_id = 1847;
	data.counter = 100;
    while (1) { 
		k_sleep(K_MSEC(60000));
		k_msgq_put(&sensor_data_queue, &data, K_FOREVER);
		//get_current_time_char();
    }
}

void spi_thread(void *unused1, void *unused2, void *unused3)
{
	uint8_t recvbuf[129];
	struct spi_data data;
	int ret;
	uint8_t counter[16];
    ret = spi_init();
    printk("SPI INIT ERROR %d\n", ret);
	while(1){

		if (k_msgq_get(&sensor_data_queue, &data, K_SECONDS(1))==0){
			sprintf(counter, "%s", data.counter);
			spi_data(data.temperature, data.humidity, data.node_id, "camu", counter, recvbuf);
			spi_sync("SYNC", recvbuf);
			if (strcmp(recvbuf, "DATA_OK") == 0) {
				printk("Data: %s\n", recvbuf);
			}
		}
		else{
			spi_sync("SYNC", recvbuf);
			if (strcmp(recvbuf, "TIME_SYNC") == 0) {
				spi_sync("TIME_SYNC", recvbuf);
				if (strlen(recvbuf) == 9) {
					spi_sync("TIME_OK", recvbuf);
					convert_time(recvbuf);
				}
			}
		}


		//handshake_toggled(data.temperature, data.humidity, data.node_id);
	}
}

int main(void)
{
    int err;

	
	k_mutex_init(&conn_mutex);

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    // k_thread_create(&bt_thread_data, bt_thread_stack_area,
    //                                        K_THREAD_STACK_SIZEOF(bt_thread_stack_area),
    //                                        bluetooth_thread,
    //                                        NULL, NULL, NULL,
    //                                        1, 0, K_NO_WAIT);

    // Start SPI thread
    k_thread_create(&spi_thread_data, spi_thread_stack_area,
                                            K_THREAD_STACK_SIZEOF(spi_thread_stack_area),
                                            spi_thread,
                                            NULL, NULL, NULL,
                                            2, 0, K_NO_WAIT);

    err = bt_sht45s_client_init(sht45s_c);
    if (err) {
        printk("SHT45 sensor Service client failed to init (err %d)\n", err);
        return 0;
    }

    scan_init();

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return 0;
    }

    printk("Scanning successfully started\n");

    return 0;

}
