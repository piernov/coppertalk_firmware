#ifndef LED_H
#define LED_H

typedef enum {
	WIFI_RED_LED, WIFI_GREEN_LED,
	UDP_RED_LED, UDP_TX_LED, UDP_RX_LED,
	LT_RED_LED, LT_TX_LED, LT_RX_LED,
	OH_NO_LED, ANY_ERR_LED, ANY_ACT_LED,
} LED;

void led_init(void);
void flash_led_once(LED led);
void flash_all_leds_once(void);
void turn_led_on(LED led);
void turn_led_off(LED led);
void turn_all_leds_on(void);
void turn_all_leds_off(void);

#endif
