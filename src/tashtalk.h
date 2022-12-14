#ifndef TASHTALK_H
#define TASHTALK_H

#include <stdbool.h>

#include "buffer_pool.h"
#include "packet_types.h"
#include "crc.h"

/* This implements the TashTalk *protocol*.  UART wrangling is handled in
   uart.c instead. */
   
   
/* a tashtalk_rx_state_t value represents a state machine for getting LLAP 
   packets out of the byte stream from TashTalk. */
typedef struct {
	buffer_pool_t* buffer_pool; // the pool to get packet buffers from
	llap_packet* packet_in_progress; // the buffer that holds the in-flight packet
	struct k_msgq *output_queue;
	bool in_escape;
	crc_state_t crc;
} tashtalk_rx_state_t;

int init_tashtalk_rx_state(tashtalk_rx_state_t *ns, buffer_pool_t* buffer_pool, 
		struct k_msgq *output_queue);
void feed(tashtalk_rx_state_t* state, unsigned char byte);
void feed_all(tashtalk_rx_state_t* state, unsigned char* buf, int count);

// tashtalk_tx_validate checks whether an LLAP packet is valid or whether
// tashtalk will choke on it (i.e. are the lengths right etc)
bool tashtalk_tx_validate(llap_packet* packet);

#endif
