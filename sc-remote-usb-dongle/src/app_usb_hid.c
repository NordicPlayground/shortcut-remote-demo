#include "app_usb_hid.h"

#include <init.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>

#include <logging/log.h>

#define LOG_LEVEL LOG_LEVEL_INF
LOG_MODULE_REGISTER(app_usb_hid);

#define HID_KBD_USAGE_CONS_CTRL_POWER				0x09, 0x30
#define HID_KBD_USAGE_CONS_CTRL_RESET				0x09, 0x31
#define HID_KBD_USAGE_CONS_CTRL_SLEEP				0x09, 0x32
#define HID_KBD_USAGE_CONS_CTRL_SCAN_NEXT_TRACK 	0x09, 0xB5
#define HID_KBD_USAGE_CONS_CTRL_SCAN_PREV_TRACK 	0x09, 0xB6
#define HID_KBD_USAGE_CONS_CTRL_PLAY_PAUSE 			0x09, 0xCD
#define HID_KBD_USAGE_CONS_CTRL_MUTE	 			0x09, 0xE2
#define HID_KBD_USAGE_CONS_CTRL_VOLUME_UP			0x09, 0xE9
#define HID_KBD_USAGE_CONS_CTRL_VOLUME_DOWN 		0x09, 0xEA
#define HID_KBD_USAGE_CONS_CTRL_AC_BACK				0x0A, 0x24, 0x02,  
#define HID_KBD_USAGE_CONS_CTRL_AC_FORWARD			0x0A, 0x25, 0x02,  

static bool configured;
static const struct device *hdev;
static ATOMIC_DEFINE(hid_ep_in_busy, 1);

#define HID_EP_BUSY_FLAG		0

#define REPORT_ID_KBD			0x01
#define REPORT_ID_CONS_CTRL		0x02
#define REPORT_SIZE_KBD			9
#define REPORT_SIZE_CONS_CTRL	2

#define REPORT_PERIOD		K_SECONDS(2)

struct report {
	uint8_t report_id;
	union {
		struct {
			uint8_t flags;
			uint8_t padding;
			uint8_t keys[6];
		} kbd;
		struct {
			uint8_t button_bitfield;
		} cons_ctrl;
	} data;
};

// Create a message queue for holding HID packets. 
K_MSGQ_DEFINE(m_hid_msg_queue, sizeof(struct report), 10, 4);

static const uint8_t hid_report_desc[] = {
		0x05, 0x01,       /* Usage Page (Generic Desktop) */
		0x09, 0x06,       /* Usage (Keyboard) */
		0xA1, 0x01,       /* Collection (Application) */

		0x85, REPORT_ID_KBD, /* Report ID 1 */
		/* Keys */
		0x05, 0x07,       /* Usage Page (Key Codes) */
		0x19, 0xe0,       /* Usage Minimum (224) */
		0x29, 0xe7,       /* Usage Maximum (231) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x01,       /* Logical Maximum (1) */
		0x75, 0x01,       /* Report Size (1) */
		0x95, 0x08,       /* Report Count (8) */
		0x81, 0x02,       /* Input (Data, Variable, Absolute) */

		0x95, 0x01,       /* Report Count (1) */
		0x75, 0x08,       /* Report Size (8) */
		0x81, 0x01,       /* Input (Constant) reserved byte(1) */

		0x95, 0x06,       /* Report Count (6) */
		0x75, 0x08,       /* Report Size (8) */
		0x15, 0x00,       /* Logical Minimum (0) */
		0x25, 0x65,       /* Logical Maximum (101) */
		0x05, 0x07,       /* Usage Page (Key codes) */
		0x19, 0x00,       /* Usage Minimum (0) */
		0x29, 0x65,       /* Usage Maximum (101) */
		0x81, 0x00,       /* Input (Data, Array) Key array(6 bytes) */

		0xC0,             /* End Collection (Application) */

		        // Report ID 2: Advanced buttons (consumer control)
        0x05, 0x0C,                     // Usage Page (Consumer)
        0x09, 0x01,                     // Usage (Consumer Control)
        0xA1, 0x01,                     // Collection (Application)
        0x85, REPORT_ID_CONS_CTRL,      //     Report Id (2)
        0x15, 0x00,                     //     Logical minimum (0)
        0x25, 0x01,                     //     Logical maximum (1)
        0x75, 0x01,                     //     Report Size (1)
        0x95, 0x01,                     //     Report Count (1)

        HID_KBD_USAGE_CONS_CTRL_VOLUME_UP,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        HID_KBD_USAGE_CONS_CTRL_VOLUME_DOWN,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        HID_KBD_USAGE_CONS_CTRL_PLAY_PAUSE,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        HID_KBD_USAGE_CONS_CTRL_MUTE,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)

        HID_KBD_USAGE_CONS_CTRL_SCAN_NEXT_TRACK,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        HID_KBD_USAGE_CONS_CTRL_SCAN_PREV_TRACK,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        HID_KBD_USAGE_CONS_CTRL_POWER,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        HID_KBD_USAGE_CONS_CTRL_SLEEP,
        0x81, 0x02,                     //     Input (Data,Value,Relative,Bit Field)
        0xC0                            // End Collection
};

static void send_report(struct report *hid_report)
{
	int ret, wrote;
	uint32_t size;

	// Set the size of the report according to the report ID
	switch(hid_report->report_id) {
		case REPORT_ID_KBD:
			size = REPORT_SIZE_KBD;
			break;
		case REPORT_ID_CONS_CTRL:
			size = REPORT_SIZE_CONS_CTRL;
			break;
		default:
			return;
	}

	// Send the packet over the HID endpoint, assuming it is not busy
	if (!atomic_test_and_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
		ret = hid_int_ep_write(hdev, (uint8_t *)hid_report, size, &wrote);
		if (ret != 0) {
			/*
			 * Do nothing and wait until host has reset the device
			 * and hid_ep_in_busy is cleared.
			 */
			LOG_ERR("Failed to submit report");
		} else {
			LOG_DBG("Report submitted");
		}
	} else {
		LOG_DBG("HID IN endpoint busy");
	}
}

static void int_in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);
	if (!atomic_test_and_clear_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG)) {
		LOG_WRN("IN endpoint callback without preceding buffer write");
	}
}

/*
 * On Idle callback is available here as an example even if actual use is
 * very limited. In contrast to report_event_handler(),
 * report value is not incremented here.
 */
static void on_idle_cb(const struct device *dev, uint16_t report_id)
{
	LOG_DBG("On idle callback");
}

static void protocol_cb(const struct device *dev, uint8_t protocol)
{
	LOG_INF("New protocol: %s", protocol == HID_PROTOCOL_BOOT ?
		"boot" : "report");
}

static const struct hid_ops ops = {
	.int_in_ready = int_in_ready_cb,
	.on_idle = on_idle_cb,
	.protocol_change = protocol_cb,
};

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	switch (status) {
	case USB_DC_RESET:
		configured = false;
		break;
	case USB_DC_CONFIGURED:
		if (!configured) {
			int_in_ready_cb(hdev);
			configured = true;
		}
		break;
	case USB_DC_SOF:
		break;
	default:
		LOG_DBG("status %u unhandled", status);
		break;
	}
}

// HID TX thread function. Used to send HID packets over USB. 
void usb_hid_tx_func(void)
{
	static struct report new_report;
	while(1) {
		// Wait until there is a new message in the queue, and read it out
		k_msgq_get(&m_hid_msg_queue, &new_report, K_FOREVER);

		// Send the new report over the HID interface
		send_report(&new_report);
	}
}

int app_usb_hid_init(void)
{
	int ret;

	LOG_INF("Initializing app_usb_hid");

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return ret;
	}

	return 0;
}

static int composite_pre_init(const struct device *dev)
{
	hdev = device_get_binding("HID_0");
	if (hdev == NULL) {
		LOG_ERR("Cannot get USB HID Device");
		return -ENODEV;
	}

	LOG_INF("HID Device: dev %p", hdev);

	usb_hid_register_device(hdev, hid_report_desc, sizeof(hid_report_desc),
				&ops);

	atomic_set_bit(hid_ep_in_busy, HID_EP_BUSY_FLAG);

	if (usb_hid_set_proto_code(hdev, HID_BOOT_IFACE_CODE_NONE)) {
		LOG_WRN("Failed to set Protocol Code");
	}

	return usb_hid_init(hdev);
}

int app_usb_hid_send_kbd_packet(uint8_t key1, uint8_t flags)
{
	int ret;
	static struct report kbd_report 
		= {.report_id = REPORT_ID_KBD, .data.kbd.keys = {0,0,0,0,0,0}};
	kbd_report.data.kbd.keys[0] = key1;
	kbd_report.data.kbd.flags = flags;
	ret = k_msgq_put(&m_hid_msg_queue, &kbd_report, K_NO_WAIT);
	return ret;
}

int app_usb_hid_send_cons_ctrl_packet(uint8_t cons_ctrl_bitfield)
{
	int ret;
	static struct report cons_ctrl_report = {.report_id = REPORT_ID_CONS_CTRL};
	cons_ctrl_report.data.cons_ctrl.button_bitfield = cons_ctrl_bitfield;
	ret = k_msgq_put(&m_hid_msg_queue, &cons_ctrl_report, K_NO_WAIT);
	return ret;
}

// Create a thread for sending HID messages to the USB stack. 
K_THREAD_DEFINE(m_thread_usb_hid_tx, 1024, usb_hid_tx_func, NULL, NULL, NULL, 5, 0, 0);

SYS_INIT(composite_pre_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);