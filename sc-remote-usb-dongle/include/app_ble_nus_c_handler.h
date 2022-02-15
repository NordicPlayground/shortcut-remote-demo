#ifndef __APP_BLE_NUS_C_HANDLER
#define __APP_BLE_NUS_C_HANDLER

#include <zephyr.h>

typedef void (*app_ble_nus_c_data_received_t)(uint8_t *data_ptr, uint32_t length);

typedef struct {
	app_ble_nus_c_data_received_t on_data_received;
} app_ble_nus_c_config_t;

int app_ble_nus_c_init(app_ble_nus_c_config_t *config);

#endif
