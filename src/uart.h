#ifndef UART_H
#define UART_H

#include <zephyr/zephyr.h>

#include "buffer_pool.h"
#include "node_table.h"

void uart_init(void);
void uart_init_tashtalk(void);
void uart_start(buffer_pool_t* packet_pool, struct k_msgq *txQueue, struct k_msgq *rxQueue);
void uart_check_for_tx_wedge(llap_packet* packet);

// get_proxy_nodes returns the node_table for the nodes that we are proxying for
// on the UART side, which is the set of nodes we have seen on the *Wifi*
// side.
node_table_t* get_proxy_nodes();


#endif
