#include <string.h>

#include "buffer_pool.h"
#include "led.h"

static const char* TAG = "bufferpool";

/* the buffer_pool_t type is defined in the header file */

void bp_panic() {
	printk("PANIC AT THE DISCO!  PANIC IN THE BUFFER POOL!\n");
	turn_led_on(OH_NO_LED);
}

int init_buffer_pool(buffer_pool_t *bp, struct k_msgq *queue, char *buffers, size_t buffer_count, size_t buffer_size) {
	// Now construct the struct to return
	if (bp == NULL) {
		goto handle_err;
	}
	bp->buffer_count = buffer_count;
	bp->buffer_size = buffer_size;
	bp->queue = queue;

	
	if (buffers == NULL) {
		goto handle_err;
	}

	// Now create the buffers
	for (size_t i = 0; i < buffer_count; i++) {
		char *buffer = buffers + i * buffer_size;
		int ret = k_msgq_put(bp->queue, &buffer, K_NO_WAIT);
		if (ret == -ENOMSG) {
			printk("BUG: in new_buffer_pool, why is the queue full?\n");
			turn_led_on(OH_NO_LED);
			goto handle_err;
		}
	}

	return 0;

handle_err:
	/* TODO: error handling */
	bp_panic();
	return -1;
}

void* bp_fetch(buffer_pool_t* bp) {
	if (bp == NULL || bp->queue->buffer_start == NULL) {
		printk("BUG: bp_fetch called on null pool or pool with null queue\n");
		turn_led_on(OH_NO_LED);
		return NULL;
	}
	
	void* buffer;

	int ret = k_msgq_get(bp->queue, &buffer, K_NO_WAIT);
	
	if (!ret) {
		return buffer;
	} else {
		printk("BUG: bp_fetch underflowed buffer pool\n");
		turn_led_on(OH_NO_LED);
		return NULL;
	}
}

void bp_relinquish(buffer_pool_t* bp, void** buff) {
	if (buff == NULL) {
		printk("BUG: bp_relinquish called on null pointer\n");
		turn_led_on(OH_NO_LED);
	}
	if (*buff == NULL) {
		printk("BUG: bp_relinquish called on pointer to null buffer\n");
		turn_led_on(OH_NO_LED);
	}

	// Try to push the buffer to the queue	
	int ret = k_msgq_put(bp->queue, buff, K_NO_WAIT);
	if (ret == -ENOMSG) {
		printk("BUG: bp_relinquish queue overflow; have you relinquished a bad pointer?\n");
		turn_led_on(OH_NO_LED);
		return;
	}

	memset(*buff, 0, bp->buffer_size);
	
	*buff = NULL;	
}

int bp_buffersize(buffer_pool_t* bp) {
	return bp->buffer_size;
}
