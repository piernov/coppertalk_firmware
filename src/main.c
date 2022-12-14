#include <zephyr/zephyr.h>

#include "packet_types.h"
#include "buffer_pool.h"
#include "led.h"
#include "network.h"
#include "ip.h"
#include "uart.h"
#include "apps.h"
#include "scan_manager.h"
#include "storage.h"
#include "recovery.h"
#include "recovery_listener.h"
#include "usb.h"

// some queues
K_MSGQ_DEFINE(UARTtoApps, sizeof(llap_packet *), 20, 4);
K_MSGQ_DEFINE(AppsToUDP, sizeof(llap_packet *), 20, 4);
K_MSGQ_DEFINE(UDPtoUART, sizeof(llap_packet *), 20, 4);

// For buffer pool
K_MSGQ_DEFINE(packet_pool_queue, sizeof(char *), 50, 4);

#if 0
void recovery_main(void) {
	storage_init();
	led_init();
	turn_all_leds_on();

	init_recovery_wifi();
	start_recovery_webserver();

	printk("would enter recovery here");
	while(1) {
		k_msleep(1000);
	}
}
#endif


void main(void)
{
	int ret = 0;
	k_thread_name_set(NULL, "main");
	printk("Initializing...\n");


	//printk("storage init\n");
	//storage_init();
	//uint8_t recovery = 0;
	//recovery = get_recovery();
	//clear_recovery();
	#if 0 // Recovery disabled for now in Zephyr port
	if (recovery) {
		recovery_main();
	}
	#endif
	

	printk("led init\n");
	led_init();
	flash_all_leds_once();
	#if 0
	printk("usb init\n");
	init_usb();
	#endif
	#if 1
	printk("uart init\n");
	uart_init();
	#endif
	printk("network init\n");
	init_network();
	#if 0 //ESP32 only
	printk("wifi init\n");
	init_at_wifi();
	#endif

	#if 1
	// A pool of buffers used for LLAP packets/frames on the way through
	static buffer_pool_t packet_pool;
	printk("sizeof(llap_packet) %u\n", sizeof(llap_packet));
	static char buffers[50][sizeof(llap_packet)];
	init_buffer_pool(&packet_pool, &packet_pool_queue, (char *)buffers[0], 50, sizeof(llap_packet));

	
	//printk("start_recovery_listener\n");
	//start_recovery_listener();	
	printk("start_udp\n");
	start_udp(&packet_pool, &AppsToUDP, &UDPtoUART);
	//printk("start_scan_manager\n");
	//start_scan_manager();
	printk("start_apps\n");
	start_apps(&packet_pool, &UARTtoApps, &UDPtoUART, &AppsToUDP);
	printk("uart_start\n");
	uart_start(&packet_pool, &UDPtoUART, &UARTtoApps);
	#endif

	printk("Initialized!\n");

	/*while(1) {
		k_msleep(1000);
	}*/
}
