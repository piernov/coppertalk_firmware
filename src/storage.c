#include <zephyr/zephyr.h>
#include <zephyr/settings/settings.h>

#include <string.h>
#include <stdbool.h>

#include "storage.h"

#include "led.h"

#define TAG "storage"

static char ssid[64] = {0};
static char pwd[64] = {0};
static uint8_t recovery;

static int handle_setting(const char *name, size_t len, void *buf, size_t buflen, settings_read_cb read_cb, void *cb_arg) {
	const char *next;
	if (settings_name_steq(name, name, &next) && !next) {
		if (len != buflen) {
			return -EINVAL;
		}

		int rc = read_cb(cb_arg, &buf, len);
		if (rc >= 0) {
			printk(TAG "loaded setting %s", name);
			return 0;
		}

		return rc;
	}
	return -ENOENT;
}

static int handler_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg) {
	int rc;

	rc = handle_setting("ssid", len, &ssid, sizeof(ssid), read_cb, cb_arg);
	if (rc != -ENOENT) {
		return rc;
	}
	rc = handle_setting("pwd", len, &pwd, sizeof(pwd), read_cb, cb_arg);
	if (rc != -ENOENT) {
		return rc;
	}
	handle_setting("recovery", len, &recovery, sizeof(recovery), read_cb, cb_arg);
	if (rc != -ENOENT) {
		return rc;
	}

	return -ENOENT;
}

static struct settings_handler handler_airtalk = {
	.name = "airtalk",
	.h_set = handler_set
};

static struct settings_handler handler_wifi = {
	.name = "wifi",
	.h_set = handler_set
};

void storage_init() {
	int rc = settings_subsys_init();
	if (rc) {
		printk(TAG "settings subsys initialization: fail (err %d)\n", rc);
		return;
	}

	printk(TAG "settings subsys initialization: OK.\n");

	rc = settings_register(&handler_airtalk);
	if (rc) {
		printk(TAG "handler airtalk register: fail (err %d)\n", rc);
	}

	rc = settings_register(&handler_wifi);
	if (rc) {
		printk(TAG "handler wifi register: fail (err %d)\n", rc);
	}

	settings_load();
}

void store_wifi_details(char* ssid_v, char* pwd_v) {
	int err = settings_save_one("wifi/ssid", ssid_v, strlen(ssid_v) + 1);
	if (err) {
		turn_led_on(OH_NO_LED);
		printk(TAG "couldn't write ssid to NVS");
	}
	err = settings_save_one("wifi/pwd", pwd_v, strlen(pwd_v) + 1);
	if (err) {
		turn_led_on(OH_NO_LED);
		printk(TAG "couldn't write pwd to NVS");
	}
	settings_commit();
}

void get_wifi_details(char* ssid_ret, size_t ssid_len, char* pwd_ret, size_t pwd_len) {
	memcpy(ssid_ret, ssid, ssid_len < sizeof(ssid) ? ssid_len : sizeof(ssid));
	memcpy(pwd_ret, pwd, pwd_len < sizeof(pwd) ? pwd_len : sizeof(pwd));
}

void store_recovery_for_next_boot(void) {
	uint8_t recovery_v = 1;
	int err = settings_save_one("airtalk/recovery", &recovery_v, 1);
	if (err) {
		turn_led_on(OH_NO_LED);
		printk(TAG "couldn't write recovery 1 to NVS");
	}
	settings_commit();
}

void clear_recovery(void) {
	uint8_t recovery_v = 0;
	int err = settings_save_one("airtalk/recovery", &recovery_v, 1);
	if (err) {
		turn_led_on(OH_NO_LED);
		printk(TAG "couldn't write recovery 0 to NVS");
	}
	settings_commit();
}

bool get_recovery(void) {
	return recovery == 1;
}
