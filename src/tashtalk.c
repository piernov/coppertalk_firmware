#include <strings.h>
#include <stdbool.h>
#include <time.h>

#include "esp_log.h"

#include "tashtalk.h"
#include "packet_types.h"
#include "crc.h"
#include "led.h"
#include "uart.h"
#include "packet_utils.h"

static const char* TAG = "tashtalk";

int init_tashtalk_rx_state(tashtalk_rx_state_t *ns, buffer_pool_t* buffer_pool, 
		struct k_msgq *output_queue) {

	if (ns == NULL) {
		printk("init_tashtalk_rx_state ns == NULL\n");
		return -1;
	}
	
	ns->buffer_pool = buffer_pool;
	ns->output_queue = output_queue;
	
	return 0;
}

void append(llap_packet* packet, unsigned char byte) {
	if (packet->length < 605) {
		packet->packet[packet->length] = byte;
		packet->length++;
	} else {
		printk("buffer overflow in packet\n");
	}
}

void do_something_sensible_with_packet(tashtalk_rx_state_t* state) {
	node_table_t* destinations = get_proxy_nodes();

   	flash_led_once(LT_RX_LED);

	// Should we forward this packet?  If it's aimed at broadcast or a node
	// we've seen on the IP side, yes
	uint8_t destination = llap_destination_node(state->packet_in_progress);
	if (destination != 255) {
		if (!nt_fresh(destinations, destination, 3600)) {
			bp_relinquish(state->buffer_pool, (void**)&state->packet_in_progress);
			return;
		}
	}
	
	uart_check_for_tx_wedge(state->packet_in_progress);
   	
	k_msgq_put(state->output_queue, &state->packet_in_progress, K_FOREVER);
}

void feed(tashtalk_rx_state_t* state, unsigned char byte) {

	//printk("->0, %d %d\n", state, state->buffer_pool);
	// do we have a buffer?
	if (state->packet_in_progress == NULL) {
		//printk("->0.1\n");
		state->packet_in_progress = bp_fetch(state->buffer_pool);
		//printk("->0.2\n");
		crc_state_init(&state->crc);
		//printk("->0.3\n");
	}
	
	//printk("->1\n");
	// buggered if I know what else to do here other than go in circles
	while (state->packet_in_progress == NULL) {
		k_msleep(10);
		state->packet_in_progress = bp_fetch(state->buffer_pool);
		crc_state_init(&state->crc);
		printk("around and around and around we go\n");
	}
	
	//printk("->2\n");
	// Are we in an escape state?
	if (state->in_escape) {
		//printk("->2.1\n");
		switch(byte) {
			case 0xFF:
				// 0x00 0xFF is a literal 0x00 byte
				append(state->packet_in_progress, 0x00);
				crc_state_append(&state->crc, 0x00);
				break;
				
			case 0xFD:
				// 0x00 0xFD is a complete frame
				if (!crc_state_ok(&state->crc)) {
					printk("/!\\ CRC fail: %d\n", state->crc);
					flash_led_once(LT_RED_LED);
				}
				
				do_something_sensible_with_packet(state);
				state->packet_in_progress = NULL;
				break;
				
			case 0xFE:
				// 0x00 0xFD is a complete frame
				printk("framing error of %d bytes\n",
					state->packet_in_progress->length);
				flash_led_once(LT_RED_LED);
				
				bp_relinquish(state->buffer_pool, (void**) &state->packet_in_progress);
				state->packet_in_progress = NULL;
				break;

			case 0xFA:
				// 0x00 0xFD is a complete frame
				printk("frame abort of %d bytes\n",
					state->packet_in_progress->length);
				flash_led_once(LT_RED_LED);
				
				bp_relinquish(state->buffer_pool, (void**) &state->packet_in_progress);
				state->packet_in_progress = NULL;
				break;			
		}
		
		state->in_escape = false;
	} else if (byte == 0x00) {
		//printk("->2.2\n");
		state->in_escape = true;
	} else {
		//printk("->2.3\n");
		append(state->packet_in_progress, byte);
		crc_state_append(&state->crc, byte);
	}
	//printk("->3\n");
}

void feed_all(tashtalk_rx_state_t* state, unsigned char* buf, int count) {
	for (int i = 0; i < count; i++) {
		feed(state, buf[i]);
	}
}

bool tashtalk_tx_validate(llap_packet* packet) {
	if (packet->length < 5) {
		// Too short
		printk("tx packet too short\n");
		return false;
	}
	
	if (packet->length == 5 && !(packet->packet[2] & 0x80)) {
		// 3 byte packet is not a control packet
		printk("tx 3-byte non-control packet, wut?\n");
		flash_led_once(LT_RED_LED);
		return false;
	}
	
	if ((packet->packet[2] & 0x80) && packet->length != 5) {
		// too long control frame
		printk("tx too-long control packet, wut?\n");
		flash_led_once(LT_RED_LED);
		return false;
	}
	
	if (packet->length == 6) {
		// impossible packet length
		printk("tx impossible packet length, wut?\n");
		flash_led_once(LT_RED_LED);
		return false;
	}
	
	if (packet->length >= 7 && (((packet->packet[3] & 0x3) << 8) | packet->packet[4]) != packet->length - 5) {
		// packet length does not match claimed length
		printk("tx length field (%d) does not match actual packet length (%d)\n", (((packet->packet[3] & 0x3) << 8) | packet->packet[4]), packet->length - 5);
		flash_led_once(LT_RED_LED);
		return false;
	}

	// check the CRC
	crc_state_t crc;
	crc_state_init(&crc);
	crc_state_append_all(&crc, packet->packet, packet->length);
	if (!crc_state_ok(&crc)) {
		printk("bad CRC on tx: IP bug?\n");
		flash_led_once(LT_RED_LED);
		return false;
	}
	
	return true;
}
