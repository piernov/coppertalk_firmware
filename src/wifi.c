#include <zephyr/zephyr.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#include <stdbool.h>
#include <string.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "led.h"
#include "storage.h"
#include "scan_manager.h"

static const char* TAG = "APP-WIFI";

bool ssid_configured = false;

#if 0
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		printk("wifi event: WIFI_EVENT_STA_START\n");
		turn_led_off(WIFI_GREEN_LED);
		turn_led_on(WIFI_RED_LED);
		//scan_blocking();
		wifi_ready = false;
		if (ssid_configured) {
			esp_wifi_connect();
		}
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		printk("wifi event: WIFI_EVENT_STA_DISCONNECTED\n");
		turn_led_off(WIFI_GREEN_LED);
		turn_led_on(WIFI_RED_LED);
		wifi_ready = false;
		//scan_blocking(); 
		//scan_blocking(); // Do a scan so we have some info when we next connect
		esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		printk("wifi event: IP_EVENT_STA_GOT_IP\n");
		turn_led_off(WIFI_RED_LED);
		turn_led_on(WIFI_GREEN_LED);
		wifi_ready = true;
	}
}
#endif

void init_at_wifi(void)
{
        wifi_if = net_if_get_default();
        if (!wifi_if) {
                printk("wifi interface not available");
                return;
        }

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	esp_err_t ret = esp_wifi_init(&cfg);
	if (ret != ESP_OK) {
		printk("esp_wifi_init failed: %d\n", ret);
	}
#if 0
	ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
	if (ret != ESP_OK) {
		printk("esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, ...) failed: %d\n", ret);
	}

	ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
	if (ret != ESP_OK) {
		printk("esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ...) failed: %d\n", ret);
	}
#endif

	wifi_config_t wifi_config = {0};
	get_wifi_details((char*)wifi_config.sta.ssid, 32, (char*)wifi_config.sta.password, 64);
	printk("details from nvs: ssid %s, pwd %s\n", wifi_config.sta.ssid, wifi_config.sta.password);


	ret = esp_wifi_set_mode(WIFI_MODE_STA);
	if (ret != ESP_OK) {
		printk("esp_wifi_set_mode failed: %d\n", ret);
	}

	ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
	if (ret != ESP_OK) {
		printk("esp_wifi_set_config failed: %d\n", ret);
	}

	ret = esp_wifi_set_ps(WIFI_PS_NONE);
	if (ret != ESP_OK) {
		printk("esp_wifi_set_ps failed: %d\n", ret);
	}

	ret = esp_wifi_connect();

	if (ret != ESP_OK) {
		printk("esp_wifi_connect failed: %d\n", ret);
	}
}
