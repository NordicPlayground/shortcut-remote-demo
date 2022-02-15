/*
 * Copyright (c) 2016-2018 Intel Corporation.
 * Copyright (c) 2018-2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>

#include "app_usb_hid.h"
#include "dk_buttons_and_leds.h"

#include <logging/log.h>

#define LOG_LEVEL LOG_LEVEL_INF
LOG_MODULE_REGISTER(main);

#define DK_BUTTON1 0
#define DK_BUTTON2 1
#define DK_BUTTON3 2
#define DK_BUTTON4 3
#define BUTTON_PRESSED(a) ((has_changed & BIT(a)) && (button_state & BIT(a)))
#define BUTTON_RELEASED(a) ((has_changed & BIT(a)) && !(button_state & BIT(a)))

static void app_button_handler(uint32_t button_state, uint32_t has_changed)
{
	if(BUTTON_PRESSED(DK_BUTTON1)) {
		app_usb_hid_send_cons_ctrl_packet(BIT(0));
	}
	if(BUTTON_PRESSED(DK_BUTTON2)) {
		app_usb_hid_send_cons_ctrl_packet(BIT(1));
	}
	if(BUTTON_PRESSED(DK_BUTTON3)) {
		static uint8_t key = KEY_A;
		app_usb_hid_send_kbd_packet(key++, 0);
		if(key > KEY_Z) key = KEY_A;
	} else if(BUTTON_RELEASED(DK_BUTTON3)) {
		app_usb_hid_send_kbd_packet(0, 0);
	}
	if(BUTTON_PRESSED(DK_BUTTON4)) {
		app_usb_hid_send_kbd_packet(KEY_M, HID_KBD_REP_FLAG_LEFT_CTRL | HID_KBD_REP_FLAG_LEFT_SHIFT);
	} else if(BUTTON_RELEASED(DK_BUTTON4)) {
		app_usb_hid_send_kbd_packet(0, 0);
	}
}

void main(void)
{
	int ret;

	LOG_INF("Starting Shortcut Remote Dongle application");

	ret = dk_buttons_init(app_button_handler);
	if(ret != 0) {
		LOG_ERR("Unable to initialize DK buttons!");
	}

	ret = app_usb_hid_init();
	if(ret != 0) {
		LOG_ERR("Unable to initialize USB HID: %d", ret);
	}
}
