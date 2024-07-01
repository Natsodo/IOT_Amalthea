/*
 * Copyright (c) 2021 Nordic Semiconductor
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef BT_HRS_CLIENT_H_
#define BT_HRS_CLIENT_H_

/**
 * @file
 * @defgroup bt_hrs_client sht45 Service Client
 * @{
 * @brief Sht45 Service Client API.
 */

#include <zephyr/bluetooth/conn.h>
#include <bluetooth/gatt_dm.h>
#include <zephyr/bluetooth/gatt.h>

#include <zephyr/sys/atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**@brief Sht45 sensor Service error codes
 *
 * This service defines the following Attribute Protocol Application error codes.
 */
enum bt_hrs_client_error {
	/** Control Point value not supported. */
	BT_HRS_CLIENT_ERROR_CP_NOT_SUPPORTED = 0x80
};


/**@brief Sht45 Measurement flags structure.
 */
struct bt_sensor_flags {
	/** Value Format flag. */
	uint8_t value_format : 1;

	/** Sensor Contact detected flag. */
	uint8_t sensor_contact_detected : 1;

	/** Sensor Contact supported flag. */
	uint8_t sensor_contact_supported : 1;

	/** Energy Expended preset flag. */
	uint8_t energy_expended_present : 1;

	/** RR-intervals present flag. */
	uint8_t rr_intervals_present : 1;
};


struct bt_buf {
    uint8_t *data;
    uint16_t len;
};

struct bt_hrs_flags {
	/** Value Format flag. */
	uint8_t value_format : 1;

	/** Sensor Contact detected flag. */
	uint8_t sensor_contact_detected : 1;

	/** Sensor Contact supported flag. */
	uint8_t sensor_contact_supported : 1;

	/** Energy Expended preset flag. */
	uint8_t energy_expended_present : 1;

	/** RR-intervals present flag. */
	uint8_t rr_intervals_present : 1;
};

struct env_data {
	/** Flags structure. */
	struct bt_sensor_flags flags;

	/** RR-intervals count. */
	uint8_t rr_intervals_count;

	/** RR-intervals represented by 1/1024 second as unit. Present if
	 * @ref bt_hrs_flags.rr_intervals_present is set. The interval with index 0 is older than
	 * the interval with index 1.
	 */
	uint16_t rr_intervals[10];

    int16_t temperature;
    uint16_t humidity;
	uint32_t node_id;
};


/**@brief Data structure of the Sensor Measurement characteristic.
 */
struct bt_sensor_client_measurement {
	/** Flags structure. */
	struct bt_sensor_flags flags;

	/** RR-intervals count. */
	uint8_t rr_intervals_count;

	/** RR-intervals represented by 1/1024 second as unit. Present if
	 * @ref bt_hrs_flags.rr_intervals_present is set. The interval with index 0 is older than
	 * the interval with index 1.
	 */
	uint16_t rr_intervals[CONFIG_BT_HRS_CLIENT_RR_INTERVALS_COUNT];  //CONFIG_BT_HRS_CLIENT_RR_INTERVALS_COUNT

	/** Sensor Measurement Value in beats per minute unit. */
	uint16_t sensor_value;

	/** Energy Expended in joule unit. Present if @ref bt_hrs_flags.energy_expended_present
	 * is set.
	 */
	uint16_t energy_expended;
};

/* Helper forward structure declaration representing Sensor Service Client instance.
 * Needed for callback declaration that are using instance structure as argument.
 */
struct bt_sensorSht_client;



/**@brief Sht45 Measurement notification callback.
 *
 * This function is called every time the client receives a notification
 * with sht45 Measurement data.
 *
 * @param[in] hrs_c sht45 Service Client instance.
 * @param[in] meas sht45 Measurement received data.
 * @param[in] err 0 if the notification is valid.
 *                Otherwise, contains a (negative) error code.
 */
typedef void (*bt_sht45s_client_notify_cb)(struct bt_sensorSht_client *sht45s_c,
					const struct env_data *meas,
					int err); //bt_sensor_client_measurement *meas


/**@brief SensorSht45 Measurement characteristic structure.
 */
struct bt_sensorSht_client_meas {
    /** Value handle. */
	uint16_t handle;

	/** Handle of the characteristic CCC descriptor. */
	uint16_t ccc_handle;

	/** GATT subscribe parameters for notification. */
	struct bt_gatt_subscribe_params notify_params;

    /** Notification callback. */
	bt_sht45s_client_notify_cb notify_cb;

};

/**@brief Sht45 Service Client instance structure.
 *        This structure contains status information for the client.
 */
struct bt_sensorSht_client {
	/** Connection object. */
	struct bt_conn *conn;

	/** Sensor data measurements characteristic. */
	struct bt_sensorSht_client_meas measurement_char;

	/** Internal state. */
	atomic_t state;
};

/**@brief Function for initializing the sht45 Service Client.
 *
 * @param[in, out] hrs_c sht45 Service Client instance. This structure must be
 *                       supplied by the application. It is initialized by
 *                       this function and will later be used to identify
 *                       this particular client instance.
 *
 * @retval 0 If the client was initialized successfully.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_sht45s_client_init(struct bt_sensorSht_client *sht45s_c);

/**@brief Subscribe to sht45 Measurement notification.
 *
 * This function writes CCC descriptor of the sht45 Measurement characteristic
 * to enable notification.
 *
 * @param[in] hrs_c sht45 Service Client instance.
 * @param[in] notify_cb   Notification callback.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_sht45s_client_measurement_subscribe(struct bt_sensorSht_client *sht45s_c,
					bt_sht45s_client_notify_cb notify_cb);

/**@brief Remove subscription to the sht45 Measurement notification.
 *
 * This function writes CCC descriptor of the sht45 Measurement characteristic
 * to disable notification.
 *
 * @param[in] hrs_c sht45 Service Client instance.
 *
 * @retval 0 If the operation was successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_sht45s_client_measurement_unsubscribe(struct bt_sensorSht_client *sht45s_c);


/**@brief Function for assigning handles to sht45 Service Client instance.
 *
 * @details Call this function when a link has been established with a peer to
 *          associate the link to this instance of the module. This makes it
 *          possible to handle several links and associate each link to a particular
 *          instance of this module.
 *
 * @param[in]     dm     Discovery object.
 * @param[in,out] hrs_c  sht45 Service Client instance for associating the link.
 *
 * @retval 0 If the operation is successful.
 *           Otherwise, a (negative) error code is returned.
 */
int bt_sht45s_client_handles_assign(struct bt_gatt_dm *dm, struct bt_sensorSht_client *sht45s_c);

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* BT_HRS_CLIENT_H_ */
