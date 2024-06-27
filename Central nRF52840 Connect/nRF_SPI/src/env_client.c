/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include "env_client.h"  
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>


LOG_MODULE_REGISTER(sht45s_client, CONFIG_BT_HRS_CLIENT_LOG_LEVEL);

#define SHTSENSOR_MEASUREMENT_NOTIFY_ENABLED BIT(0)
#define HRS_MEASUREMENT_READ_IN_PROGRES BIT(1)
#define HRS_SENSOR_LOCATION_READ_IN_PROGRES BIT(2)
#define HRS_CONTROL_POINT_WRITE_PENDING BIT(3)

#define HRS_MEASUREMENT_FLAGS_VALUE_FORMAT      BIT(0)
#define HRS_MEASUREMENT_FLAGS_CONTACT_DETECTED  BIT(1)
#define HRS_MEASUREMENT_FLAGS_CONTACT_SUPPORTED BIT(2)
#define HRS_MEASUREMENT_FLAGS_ENERGY_EXPENDED   BIT(3)
#define HRS_MEASUREMENT_FLAGS_RR_INTERVALS      BIT(4)



static inline bool data_length_check(uint16_t current, uint16_t min_expected)
{
	return (current >= min_expected);
}

static void sht45s_reinit(struct bt_sensorSht_client *sht45s_c)
{
	sht45s_c->measurement_char.handle = 0;
	sht45s_c->measurement_char.ccc_handle = 0;
	sht45s_c->measurement_char.notify_cb = NULL;

	sht45s_c->conn = NULL;
	sht45s_c->state = ATOMIC_INIT(0);
}


static int sht45s_measurement_data_parse(struct env_data *data, const uint8_t *data_array, uint16_t length)
{
  
    if (data_array == NULL || data == NULL) {
        return -EINVAL;
    }

    if (data_array[0] < 8) {
        return -EMSGSIZE; 
    }

    uint16_t humidity_value  = (data_array[1] << 8 | data_array[0]);
    
    int16_t temperature_value  = (data_array[3] << 8 | data_array[2]); 
   
	uint32_t node_id = (data_array[7] << 24 | data_array[6] << 16| data_array[5] << 8| data_array[4]);

	data->node_id = node_id;
	data->humidity = humidity_value;
	data->temperature = temperature_value;
    return 0;
}



static uint8_t on_sht45s_measurement_notify(struct bt_conn *conn,
					 struct bt_gatt_subscribe_params *params,
					 const void *data, uint16_t length)
{
	int err;
	struct bt_sensorSht_client *sht45s_c;
    struct env_data received_data;

	sht45s_c = CONTAINER_OF(params, struct bt_sensorSht_client, measurement_char.notify_params);

	if (!data) {
		atomic_clear_bit(&sht45s_c->state, SHTSENSOR_MEASUREMENT_NOTIFY_ENABLED);
		LOG_DBG("[UNSUBSCRIBE] from sensor Measurement characterictic");

		return BT_GATT_ITER_STOP;
	}

	LOG_HEXDUMP_DBG(data, length, "[NOTIFICATION] sensor Measurement: ");


	err = sht45s_measurement_data_parse(&received_data, data, length);

	if (sht45s_c->measurement_char.notify_cb) {
		sht45s_c->measurement_char.notify_cb(sht45s_c, &received_data, err);
	}

	return BT_GATT_ITER_CONTINUE;
}



int bt_sht45s_client_measurement_subscribe(struct bt_sensorSht_client *sht45s_c,
					bt_sht45s_client_notify_cb notify_cb)                         
{

	int err;
	struct bt_gatt_subscribe_params *params = &sht45s_c->measurement_char.notify_params;

	if (!sht45s_c || !notify_cb) {
		return -EINVAL;
	}

	if (atomic_test_and_set_bit(&sht45s_c->state, SHTSENSOR_MEASUREMENT_NOTIFY_ENABLED)) {
		LOG_ERR("Sensor Measurement characterisic notifications already enabled.");
		return -EALREADY;
	}

	sht45s_c->measurement_char.notify_cb = notify_cb;

	params->ccc_handle = sht45s_c->measurement_char.ccc_handle;
	params->value_handle = sht45s_c->measurement_char.handle;
	params->value = BT_GATT_CCC_NOTIFY;
	params->notify = on_sht45s_measurement_notify;

	atomic_set_bit(params->flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);

	err = bt_gatt_subscribe(sht45s_c->conn, params);
	if (err) {
		atomic_clear_bit(&sht45s_c->state, SHTSENSOR_MEASUREMENT_NOTIFY_ENABLED);
		LOG_ERR("Subscribe to sensor Measurement characteristic failed");
	} else {
		LOG_DBG("Subscribed to sensor Measurement characteristic");
	}

	return err;
}

int bt_sht45s_client_measurement_unsubscribe(struct bt_sensorSht_client *sht45s_c)
{
	int err;

	if (!sht45s_c) {
		return -EINVAL;
	}

	if (!atomic_test_bit(&sht45s_c->state, SHTSENSOR_MEASUREMENT_NOTIFY_ENABLED)) {
		return -EFAULT;
	}

	err = bt_gatt_unsubscribe(sht45s_c->conn, &sht45s_c->measurement_char.notify_params);
	if (err) {
		LOG_ERR("Unsubscribing from sensor Measurement characteristic failed, err: %d",
			err);
	} else {
		atomic_clear_bit(&sht45s_c->state, SHTSENSOR_MEASUREMENT_NOTIFY_ENABLED);
		printk("Unsubscribed from sensor Measurement characteristic");
	}

	return err;
}



int bt_sht45s_client_handles_assign(struct bt_gatt_dm *dm, struct bt_sensorSht_client *sht45s_c)
{
	const struct bt_gatt_dm_attr *gatt_service_attr =
			bt_gatt_dm_service_get(dm);
	const struct bt_gatt_service_val *gatt_service =
			bt_gatt_dm_attr_service_val(gatt_service_attr);
	const struct bt_gatt_dm_attr *gatt_chrc;
	const struct bt_gatt_dm_attr *gatt_desc;

	if (!dm || !sht45s_c) {
		return -EINVAL;
	}

	if (bt_uuid_cmp(gatt_service->uuid, BT_UUID_ESS)) {   
		return -ENOTSUP;
	}
	LOG_DBG("Getting handles from SHT45 sensor service.");

	sht45s_reinit(sht45s_c);



	gatt_chrc = bt_gatt_dm_char_by_uuid(dm, BT_UUID_TEMPERATURE);
	if (!gatt_chrc) {
		LOG_ERR("No sensorSHT45 characteristic found.");
		return -EINVAL;
	}


    gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc,
					    BT_UUID_TEMPERATURE);
	if (!gatt_desc) {
		LOG_ERR("No sensorSHT45 characteristic value found.");
		return -EINVAL;
	}
	sht45s_c->measurement_char.handle = gatt_desc->handle;

	gatt_desc = bt_gatt_dm_desc_by_uuid(dm, gatt_chrc, BT_UUID_GATT_CCC);
	if (!gatt_desc) {
		LOG_ERR("No SHT45 Measurement CCC descriptor found.");
		return -EINVAL;
	}

	sht45s_c->measurement_char.ccc_handle = gatt_desc->handle;

	LOG_DBG("SHT45 Measurement characteristic found");

    

	/* Finally - save connection object */
	sht45s_c->conn = bt_gatt_dm_conn_get(dm);

	return 0;
}

int bt_sht45s_client_init(struct bt_sensorSht_client *sht45s_c)
{
	if (!sht45s_c) {
		return -EINVAL;
	}

	memset(sht45s_c, 0, sizeof(*sht45s_c));

	return 0;
}

