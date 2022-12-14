#include <zephyr/zephyr.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/gpio.h>

#include "esp_log.h"

#include "led.h"
#include "storage.h"

#include "recovery_listener.h"

#define THREAD_RECOVERY_LISTENER_PRIORITY CONFIG_NUM_PREEMPT_PRIORITIES - 1// Lowest priority
static const struct gpio_dt_spec recoverybutton = GPIO_DT_SPEC_GET(DT_ALIAS(recoverybutton), gpios);

struct k_thread recovery_listener_task;
K_THREAD_STACK_DEFINE(recovery_listener_stack, 2048);

void recovery_listener_runloop(void *p1, void *p2, void *p3) {
	int last_recovery = 1;
	int recovery;

	while(1) {
		// This is a hack but it's simple and unintrusive.
		k_msleep(800);
		
		recovery = gpio_pin_get_dt(&recoverybutton);
		if (recovery == 0 && last_recovery == 0) {
			// start recovery mode
			store_recovery_for_next_boot();
			sys_reboot(SYS_REBOOT_WARM);
		}
		
		last_recovery = recovery;
	}

}

void start_recovery_listener(void) {
	gpio_pin_configure_dt(&recoverybutton, GPIO_INPUT);
	
	k_thread_create(&recovery_listener_task, recovery_listener_stack,
		K_THREAD_STACK_SIZEOF(recovery_listener_stack),
		recovery_listener_runloop,
		NULL, NULL, NULL,
		THREAD_RECOVERY_LISTENER_PRIORITY, 0, K_NO_WAIT);
}
