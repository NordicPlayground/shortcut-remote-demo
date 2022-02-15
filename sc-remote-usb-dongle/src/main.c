/*
 * Copyright (c) 2016-2018 Intel Corporation.
 * Copyright (c) 2018-2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include "app_usb_hid.h"

#include <logging/log.h>

#define LOG_LEVEL LOG_LEVEL_INF
LOG_MODULE_REGISTER(main);

void main(void)
{
	int ret;

	LOG_INF("Starting Shortcut Remote Dongle application");

	ret = app_usb_hid_init();
	if(ret != 0) {
		LOG_ERR("Unable to initialize USB HID: %d", ret);
	}

	while(1) {
		static uint8_t my_key = KEY_A;
		ret = app_usb_hid_send_kbd_packet(my_key++, 0);
		if(ret != 0) {
			LOG_ERR("Unable to send HID keyboard packet");
		}
		k_msleep(2000);
	}
}
