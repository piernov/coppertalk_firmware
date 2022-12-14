#include <zephyr/zephyr.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>

#include <stm32f1xx_ll_usart.h>
#include <stm32f1xx_ll_gpio.h>

#include <time.h>
#include <string.h>
#include <stdio.h>

#include "led.h"
#include "tashtalk.h"
#include "node_table.h"
#include "packet_types.h"
#include "packet_utils.h"

#include "uart.h"

#define THREAD_UART_RX_PRIORITY 4
#define THREAD_UART_TX_PRIORITY 3

static const struct gpio_dt_spec ttcts = GPIO_DT_SPEC_GET(DT_ALIAS(tt_cts), gpios);
//static const struct gpio_dt_spec ttcts2 = GPIO_DT_SPEC_GET(DT_ALIAS(tt_cts2), gpios);


static const char* TAG = "uart";

const struct device *uart_dev = DEVICE_DT_GET(DT_ALIAS(uart_tt)); //FIXME: correct uart

node_table_t node_table = { 0 };
node_update_packet_t node_table_packet = { 0 };

struct k_thread uart_rx_task;
struct k_thread uart_tx_task;
struct k_msgq *uart_rx_queue = NULL;
struct k_msgq *uart_tx_queue = NULL;

K_THREAD_STACK_DEFINE(uart_rx_stack, 512);
K_THREAD_STACK_DEFINE(uart_tx_stack, 512);

#define RX_BUFFER_SIZE 768
K_MSGQ_DEFINE(uart_msgq, sizeof(char), RX_BUFFER_SIZE, 1); // Store each character separately in the queue, not the most efficient but hopefully good enough

void uart_write_node_table(node_update_packet_t* packet) {
	//printk("BEGIN uart_write_node_table\n");
	uart_poll_out(uart_dev, '\x02');
	for (size_t i = 0; i < 32; i++) {
		uart_poll_out(uart_dev, packet->nodebits[i]);
	}
	//uart_tx(uart_dev, "\x01", 1, SYS_FOREVER_US);
	//uart_tx(uart_dev, (char*)packet->nodebits, 32, SYS_FOREVER_US);
	
	// debugomatic
	static char debugbuff[97] = {0};
	char* ptr = debugbuff;
	for (int i = 0; i < 32; i++) {
		ptr += sprintf(ptr, "%02X ", packet->nodebits[i]);
	}
	printk("sent nodebits: %s\n", debugbuff);
	//printk("END uart_write_node_table \n");
}

node_table_t* get_proxy_nodes() {
	return &node_table;
}

void uart_init_tashtalk(void) {
	printk("Initializing TashTalk...\n");
	//char init[1024] = { 0 };
	uart_write_node_table(&node_table_packet);
	for (size_t i = 0; i < 1024; i++) {
		//uart_poll_out(uart_dev, init[i]);
		uart_poll_out(uart_dev, '\0');
	}
	//uart_tx(uart_dev, init, 1024, SYS_FOREVER_US);
	printk("TashTalk Initialized.\n");
}

void uart_check_for_tx_wedge(llap_packet* packet) {
	// Sometimes, if buffer weirdness happens, tashtalk gets wedged.  In those
	// cases, we need to detect it.  We can detect this by looking for strings
	// of handshaking requests.
	//
	// This works because we know that we should respond to a CTS for one of our
	// nodes.  If we get more than one burst of CTSes, something's gone wrong
	// and we should reset the tashtalk to a known state.
	
	static int handshake_count = 0;
	if (llap_type(packet) == 0x84 || llap_type(packet) == 0x85) {
		handshake_count++;
		
		if (handshake_count > 10) {
			flash_led_once(LT_RED_LED);
			printk("%d consecutive handshakes! reinitialising tashtalk\n", handshake_count);
			uart_init_tashtalk();
			handshake_count = 0;			
		}
	} else {
		handshake_count = 0;
	}
}

void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	while (uart_irq_rx_ready(uart_dev)) {
		uart_fifo_read(uart_dev, &c, 1);
		/* if queue is full, message is silently dropped, not much else to do since we're in an ISR */
		k_msgq_put(&uart_msgq, &c, K_NO_WAIT);
	}
}

void uart_init(void) {	
	const struct uart_config uart_config = {
		.baudrate = 1000000,
		.parity = UART_CFG_PARITY_NONE,
		.data_bits = UART_CFG_DATA_BITS_8,
		.stop_bits = UART_CFG_STOP_BITS_1,
		//.flow_ctrl = UART_CFG_FLOW_CTRL_RTS_CTS,
		//.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	//struct device *gpiob = device_get_binding("GPIOB");
	int ret = gpio_pin_configure_dt(&ttcts, GPIO_INPUT);
	//ret += gpio_pin_interrupt_configure_dt(&ttcts, GPIO_INT_EDGE_TO_ACTIVE);

	//ret += gpio_pin_configure_dt(&ttcts2, GPIO_INPUT | GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		printk("gpio_pin_configure_dt %d \n", ret);
	}
	//gpio_init_callback(&ttcts_int_cb, ttcts_cb, BIT(ttcts.pin));
	//if (gpio_add_callback(ttcts.port, &ttcts_int_cb) < 0) {
	//	printk("ttcts cb failed\n");
	//}
	printk("Set up ttcts at %s pin %d\n", ttcts.port->name, ttcts.pin);
	//gpio_pin_enable_callback(gpiob, 14);



	printk("Configuring UART...\n");
	//int ret = uart_configure(uart_dev, &uart_config);
	//if (ret) {
	//	printk("uart_configure error: %d\n", ret);
	//}
	



  /* Configure CTS Pin as : Alternate function, High Speed, OpenDrain, Pull up */
/*
  LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_14, LL_GPIO_MODE_ALTERNATE);
  LL_GPIO_SetPinSpeed(GPIOB, LL_GPIO_PIN_14, LL_GPIO_SPEED_FREQ_HIGH);
  LL_GPIO_SetPinOutputType(GPIOB, LL_GPIO_PIN_14, LL_GPIO_OUTPUT_OPENDRAIN);
  LL_GPIO_SetPinPull(GPIOB, LL_GPIO_PIN_14, LL_GPIO_PULL_UP);



	LL_USART_SetHWFlowCtrl(USART1, LL_USART_HWCONTROL_RTS);
*/

	/* configure interrupt and callback to receive data */
	uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
	uart_irq_rx_enable(uart_dev);


	printk("UART configured.\n");

	
	//ESP_ERROR_CHECK(uart_set_pin(uart_num, UART_TX, UART_RX, UART_RTS, UART_CTS));
	//ESP_ERROR_CHECK(uart_driver_install(uart_num, 2048, 0, 0, NULL, 0));

	uart_init_tashtalk();

	printk("uart_init complete\n");
}

void uart_rx_runloop(void* buffer_pool, void *p2, void *p3) {
	static const char *TAG = "UART_RX";	
	printk("uart_rx_runloop started\n");
	
	tashtalk_rx_state_t rxstate = {0};
	int ret = init_tashtalk_rx_state(&rxstate, (buffer_pool_t*)buffer_pool, uart_rx_queue);
	
	while(1){
		char c;

		if (k_msgq_get(&uart_msgq, &c, K_FOREVER) == 0) {
			feed(&rxstate, c);
		}

#if 0
		/* TODO: this is stupid, read a byte at a time instead and wait for MAX_DELAY */
		char c;
		int ret = uart_poll_in(uart_dev, &c);
		if (!ret) {
			//printk("Received char on uart: %c\n", c);
			feed(&rxstate, c);
		} else {
			k_msleep(1); //FIXME: probably a bad idea.
		}
#endif
	}
}

void uart_tx_runloop(void* buffer_pool, void *p2, void *p3) {
	llap_packet* packet = NULL;
	static const char *TAG = "UART_TX";
	
	printk("started\n");
	
	while(1){
		/* TODO: we should time out here every so often to re-calculate stale
		   nodes in the node table */
		k_msgq_get(uart_tx_queue, &packet, K_FOREVER);
		
		// validate the packet: first, check the CRC:
		crc_state_t crc;
		crc_state_init(&crc);
		crc_state_append_all(&crc, packet->packet, packet->length);
		
		if (!tashtalk_tx_validate(packet)) {
			printk("packet validation failed\n");
			flash_led_once(LT_RED_LED);
			goto skip_processing;
		}
		
		// Now mark the sender as active and see if we need to update tashtalk's
		// node mask
		nt_touch(&node_table, packet->packet[1]);
		bool changed = nt_serialise(&node_table, &node_table_packet, 1800);
		if (changed) {
			printk("node table changed!\n");
			uart_write_node_table(&node_table_packet);
		}
		
		uart_poll_out(uart_dev, '\x01');
		for (size_t i = 0; i < packet->length; i++) {
			while (gpio_pin_get_dt(&ttcts)) {}
			uart_poll_out(uart_dev, packet->packet[i]);
		}
		//uart_tx(uart_dev, "\x01", 1, SYS_FOREVER_US);
		//uart_tx(uart_dev, (const char*)packet->packet, packet->length, SYS_FOREVER_US);
		
		flash_led_once(LT_TX_LED);
		
skip_processing:
		//printk("bp_relinquish uart\n");
		bp_relinquish((buffer_pool_t*)buffer_pool, (void**)&packet);
	}
}

void uart_start(buffer_pool_t* packet_pool, struct k_msgq *txQueue, struct k_msgq *rxQueue) {
	uart_rx_queue = rxQueue;
	uart_tx_queue = txQueue;
	k_tid_t uart_rx_thread = k_thread_create(&uart_rx_task, uart_rx_stack,
			K_THREAD_STACK_SIZEOF(uart_rx_stack),
			uart_rx_runloop,
			(void*)packet_pool, NULL, NULL,
			THREAD_UART_RX_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(uart_rx_thread, "uart_rx");
	k_tid_t uart_tx_thread = k_thread_create(&uart_tx_task, uart_tx_stack,
			K_THREAD_STACK_SIZEOF(uart_tx_stack),
			uart_tx_runloop,
			(void*)packet_pool, NULL, NULL,
			THREAD_UART_TX_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(uart_tx_thread, "uart_tx");
}
