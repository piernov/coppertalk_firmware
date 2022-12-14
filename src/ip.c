#include <zephyr/zephyr.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/igmp.h>

#include "esp_log.h"

#include "packet_types.h"
#include "buffer_pool.h"
#include "wifi.h"
#include "led.h"
#include "crc.h"
#include "uart.h"
#include "node_table.h"

#define THREAD_UDP_RX_PRIORITY 5
#define THREAD_UDP_TX_PRIORITY 5

static const char* TAG = "ip";

int udp_sock = -1;
buffer_pool_t* packet_pool;
struct k_thread udp_rx_task;
struct k_thread udp_tx_task;

struct k_msgq *udp_tx_queue = NULL;
struct k_msgq *udp_rx_queue = NULL;

K_THREAD_STACK_DEFINE(udp_rx_stack, 4096);
K_THREAD_STACK_DEFINE(udp_tx_stack, 4096);

node_table_t forward_for_nodes = { 0 };

void udp_error() {
	flash_led_once(UDP_RED_LED);
}

void init_udp(void) {
	struct sockaddr_in saddr = { 0 };
	int sock = -1;
	int err = 0;
	
	/* bail out early if we don't have a wifi network interface yet */
	
	struct net_if * wifi_if = get_wifi_intf();
	if (wifi_if == NULL) {
		printk("wireless network interface does not exist.\n");
		udp_error();
		return;
	}
	
	/* or if doesn't think it's ready */
	if (!is_wifi_ready()) {
		printk("wireless network interface is not ready.\n");
		udp_error();
		return;
	};
	
	/* create the multicast socket and twiddle its socket options */

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock < 0) {
		printk("socket() failed.  error: %d\n", errno);
		udp_error();
		return;
	}
	
	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(1954);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	if (err < 0) {
		printk("bind() failed, error: %d\n", errno);
		udp_error();
		goto err_cleanup;
	}
		
	struct sockaddr_in addr;
	err = inet_pton(AF_INET, "239.192.76.84", &addr.sin_addr);
	if (err != 1) {
		printk("inet_pton failed, error: %d\n", errno);
		udp_error();
		goto err_cleanup;
	}

	struct net_if_mcast_addr *ret = net_if_ipv4_maddr_add(wifi_if, &addr.sin_addr);
	if (ret == NULL) {
		printk("net_if_ipv4_maddr_add() failed, error: %d\n", errno);
		udp_error();
		goto err_cleanup;
	}

	err = net_ipv4_igmp_join(wifi_if, &addr.sin_addr);
	if (err) {
		printk("net_ipv4_igmp_join error: %d\n", err);
	}
	 
	udp_sock = sock;
	
	printk("multicast socket now ready.\n");
						
	return;

	
err_cleanup:
	close(sock);
	udp_error();
	return;

}

void udp_rx_runloop(void *p1, void *p2, void *p3) {
	printk("starting LToUDP listener\n");
	init_udp();
	
	while(1) {
		/* retry if we need to */
		while(udp_sock == -1) {
			printk("setting up LToUDP listener failed.  Retrying...\n");
			k_msleep(1000);
			init_udp();
		}
	
		while(1) {
			unsigned char buffer[603+4]; // 603 for LLAP (per Inside Appletalk) + 
										 // 4 bytes LToUDP header
			int len = recv(udp_sock, buffer, sizeof(buffer), 0);
			//printk("Received UDP packet\n");
			if (len < 0) {
				flash_led_once(UDP_RED_LED);
				break;
			}
			if (len > 609) {
				flash_led_once(UDP_RED_LED);
				printk("packet too long: %d\n", len);
				continue;
			}
			if (len > 7) {
				flash_led_once(UDP_RX_LED);
			
				// Have we seen any traffic from this node?  Is it one
				// we actually want to send traffic to?  If not, go around
				// again.
				if (!nt_fresh(&forward_for_nodes, buffer[4], 3600) &&
					(buffer[4] != 255)) {
					continue;
				}
			
				// fetch an empty buffer from the pool and fill it with
				// packet info
				llap_packet* lp = bp_fetch(packet_pool);
				if (lp != NULL) {
					crc_state_t crc;
				
					// copy the LLAP packet into the packet buffer
					lp->length = len-2; // -4 for LToUDP tag, +2 for the FCS
					memcpy(lp->packet, buffer+4, len-4);
				
					// now calculate the CRC
					crc_state_init(&crc);
					crc_state_append_all(&crc, buffer+4, len-4);
					lp->packet[lp->length - 2] = crc_state_byte_1(&crc);
					lp->packet[lp->length - 1] = crc_state_byte_2(&crc);
				
					k_msgq_put(udp_rx_queue, &lp, K_FOREVER);
				} else {
					printk("udp_rx_runloop lp == NULL\n");
				}
			}			
		}
	
		close(udp_sock);
		udp_sock = -1;
	}
}

void udp_tx_runloop(void *p1, void *p2, void *p3) {
	//printk("udp_tx_runloop 0\n");
	llap_packet* packet = NULL;
	unsigned char outgoing_buffer[605 + 4] = { 0 }; // LToUDP has 4 byte header
	struct sockaddr_in dest_addr = {0};
	int err = 0;

	//printk("udp_tx_runloop 1\n");
		
	err = inet_pton(AF_INET, "239.192.76.84", &dest_addr.sin_addr);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(1954);
	
	ESP_LOGI(TAG, "starting LToUDP sender");
	
	while(1) {
		k_msgq_get(udp_tx_queue, &packet, K_FOREVER);
				
		//printk("udp_tx_runloop 2.1\n");
		if (packet == NULL) {
			continue;
		}
		
		// Add this to the "known senders" list we're forwarding traffic for
		if (packet->length > 3) {
			nt_touch(&forward_for_nodes, packet->packet[1]);
		}
		//printk("udp_tx_runloop 2.2\n");
		
		// Should we pass this on?
		// If it's an RTS or a CTS packet, then no
		if (packet->length == 5) {
			if (packet->packet[2] == 0x84 || packet->packet[2] == 0x85) {
				goto skip_processing;
			}
		}
				
		//printk("udp_tx_runloop 2.3\n");
		/* TODO: if this is an ENQ for a node ID that is not in our recently
		   seen table, pass it on, otherwise block all ENQs */
		   
		if (udp_sock != -1) {
			memcpy(outgoing_buffer+4, packet->packet, packet->length);
			//printk("udp_tx_runloop 2.3.1\n");
			
			err = sendto(udp_sock, outgoing_buffer, packet->length+2, 0, 
				(struct sockaddr *)&dest_addr, sizeof(dest_addr));
			//printk("udp_tx_runloop 2.3.2\n");
			if (err < 0) {
				ESP_LOGE(TAG, "error: sendto: errno %d", errno);
				flash_led_once(UDP_RED_LED);
			} else {
				flash_led_once(UDP_TX_LED);
			}
			err = 0;
			//printk("udp_tx_runloop 2.3.3\n");
		}
				
skip_processing:
		bp_relinquish(packet_pool, (void**)&packet);
		//printk("udp_tx_runloop 2.4\n");
	}
	//printk("udp_tx_runloop 3\n");
}


void start_udp(buffer_pool_t* pool, struct k_msgq *txQueue, struct k_msgq *rxQueue) {
	packet_pool = pool;
	udp_tx_queue = txQueue;
	udp_rx_queue = rxQueue;
        k_thread_create(&udp_rx_task, udp_rx_stack,
                        K_THREAD_STACK_SIZEOF(udp_rx_stack),
                        udp_rx_runloop,
                        NULL, NULL, NULL,
                        THREAD_UDP_RX_PRIORITY, 0, K_NO_WAIT);
        k_thread_create(&udp_tx_task, udp_tx_stack,
                        K_THREAD_STACK_SIZEOF(udp_tx_stack),
                        udp_tx_runloop,
                        NULL, NULL, NULL,
                        THREAD_UDP_TX_PRIORITY, 0, K_NO_WAIT);


}
