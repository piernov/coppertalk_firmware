#include <zephyr/zephyr.h>
#include <zephyr/drivers/gpio.h>

#include "led.h"

#define FLASH_LENGTH 125
#define THREAD_LED_PRIORITY CONFIG_NUM_PREEMPT_PRIORITIES - 2// Second to lowest priority

static const char* TAG = "LED";
void led_runloop(void* param, void *p2, void *p3);

/* Configuration for the status LEDs */
typedef struct {
	/* initialise these fields */
	
	int enabled; 
	char* name;
	const struct gpio_dt_spec gpio_pin;
	int also;
	
	/* don't initialise these */
	
	struct k_thread thread;
	struct k_event event;
	int gpio_state;
} led_config_t;

//#define LED_COUNT 11
#define LED_COUNT 9
//#define LED_COUNT 1

static led_config_t led_config[LED_COUNT] = {
	[WIFI_RED_LED] = {
		.enabled = 1,
		.name = "WIFI_RED_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(wifi_red_led), gpios)
	},
#if 1
	[WIFI_GREEN_LED] = {
		.enabled = 1,
		.name = "WIFI_GREEN_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(wifi_green_led), gpios)
	},
	
	[UDP_RED_LED] = {
		.enabled = 1,
		.name = "UDP_RED_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(udp_red_led), gpios),
//		.also = ANY_ERR_LED
	},
	
	[UDP_TX_LED] = {
		.enabled = 1,
		.name = "UDP_TX_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(udp_tx_led), gpios),
//		.also = ANY_ACT_LED
	},
	
	[UDP_RX_LED] = {
		.enabled = 1,
		.name = "UDP_RX_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(udp_rx_led), gpios),
//		.also = ANY_ACT_LED
	},
	
	[LT_RED_LED] = {
		.enabled = 1,
		.name = "LT_RED_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(lt_red_led), gpios),
//		.also = ANY_ERR_LED
	},
	
	[LT_TX_LED] = {
		.enabled = 1,
		.name = "LT_TX_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(lt_tx_led), gpios),
//		.also = ANY_ACT_LED
	},
	
	[LT_RX_LED] = {
		.enabled = 1,
		.name = "LT_RX_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(lt_rx_led), gpios),
//		.also = ANY_ACT_LED
	},
	
	[OH_NO_LED] = {
		.enabled = 1,
		.name = "OH_NO_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(oh_no_led), gpios)
	},
#endif
#if 0
	[ANY_ERR_LED] = {
		.enabled = 1,
		.name = "ANY_ERR_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(generic_err_led), gpios)
	},
	
	[ANY_ACT_LED] = {
		.enabled = 1,
		.name = "ANY_ACT_LED",
		.gpio_pin = GPIO_DT_SPEC_GET(DT_ALIAS(generic_act_led), gpios)
	},
#endif
};

K_THREAD_STACK_ARRAY_DEFINE(led_stacks, LED_COUNT, 256);

/* led_init sets up gpio pins and starts the background tasks for each LED */
void led_init(void) {
	int num_of_leds = sizeof(led_config) / sizeof(led_config[0]);
	
	/* set up GPIO pin, light up each LED */
	
	for(int i = 0; i < num_of_leds; i++) {
		if (led_config[i].enabled != 1) {
			continue;
		}
		
		gpio_pin_configure_dt(&led_config[i].gpio_pin, GPIO_OUTPUT_ACTIVE);
	}
	
	k_msleep(250);
	
	/* Now turn off each LED and start the background task for each LED */
	for(int i = 0; i < num_of_leds; i++) {
		if (led_config[i].enabled != 1) {
			continue;
		}
	
		gpio_pin_set_dt(&led_config[i].gpio_pin, 0);

		k_tid_t tid = k_thread_create(&led_config[i].thread, led_stacks[i],
						 K_THREAD_STACK_SIZEOF(led_stacks[i]),
						 led_runloop,
						 (void *)i, NULL, NULL,
						 THREAD_LED_PRIORITY, 0, K_NO_WAIT);
		k_thread_name_set(tid, led_config[i].name);
		k_event_init(&led_config[i].event);
	}
}


void led_runloop(void* param, void *p2, void *p3) {
	LED led = (LED) param;
	unsigned int dummy_value = 0;
	
	/* We just run in a loop, waiting to be notified, flashing our little
	   light, then going back to sleep again. */
	while(1) {
		k_event_wait(&led_config[led].event, 0x1, true, K_FOREVER);
		gpio_pin_set_dt(&led_config[led].gpio_pin, (led_config[led].gpio_state + 1) % 2);
		//printk("LED %d on\n", led);
		k_msleep(FLASH_LENGTH);
		gpio_pin_set_dt(&led_config[led].gpio_pin, led_config[led].gpio_state);
		//printk("LED %d off\n", led);
	}
}

bool check_led(LED led) {
	static const int num_of_leds = sizeof(led_config) / sizeof(led_config[0]);
	if (led >= num_of_leds || !led_config[led].enabled) {
		printk("attempt to flash illicit LED: %d\n", led);
		return false;
	}
	return true;
}

void turn_led_on(LED led) {
	if (!check_led(led)) return;
	led_config[led].gpio_state = 1;
	gpio_pin_set_dt(&led_config[led].gpio_pin, 1);
}

void turn_led_off(LED led) {
	if (!check_led(led)) return;
	led_config[led].gpio_state = 0;
	gpio_pin_set_dt(&led_config[led].gpio_pin, 0);
}


void flash_led_once(LED led) {
	if (!check_led(led)) return;
	
	k_event_post(&led_config[led].event, 0x1);
	
	// do we have a second LED we should also flash?
	if (led_config[led].also) {
		flash_led_once(led_config[led].also);
	}
}

void flash_all_leds_once(void) {
	int num_of_leds = sizeof(led_config) / sizeof(led_config[0]);
	
	for(int i = 0; i < num_of_leds; i++) {
		if (led_config[i].enabled != 1) {
			continue;
		}
		
		flash_led_once(i);
	}
}

void turn_all_leds_on(void) {
	int num_of_leds = sizeof(led_config) / sizeof(led_config[0]);
	
	for(int i = 0; i < num_of_leds; i++) {
		if (led_config[i].enabled != 1) {
			continue;
		}
		
		turn_led_on(i);
	}
}

void turn_all_leds_off(void) {
	int num_of_leds = sizeof(led_config) / sizeof(led_config[0]);
	
	for(int i = 0; i < num_of_leds; i++) {
		if (led_config[i].enabled != 1) {
			continue;
		}
		
		turn_led_off(i);
	}
}
