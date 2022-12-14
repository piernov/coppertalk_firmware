#include <zephyr/zephyr.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#include <stdbool.h>
#include <string.h>

#include "led.h"
#include "storage.h"

struct net_if *iface = NULL;
static bool network_ready = false;

static struct net_mgmt_event_callback dhcp_cb;

static void handler_cb(struct net_mgmt_event_callback *cb,
                    uint32_t mgmt_event, struct net_if *iface)
{
        if (mgmt_event != NET_EVENT_IPV4_DHCP_BOUND) {
                return;
        }

	turn_led_on(WIFI_GREEN_LED);
	turn_led_off(WIFI_RED_LED);
	network_ready = true;

        char buf[NET_IPV4_ADDR_LEN];

        printk("Your address: %s\n",
                net_addr_ntop(AF_INET,
                                   &iface->config.dhcpv4.requested_ip,
                                   buf, sizeof(buf)));
        printk("Lease time: %u seconds\n",
                        iface->config.dhcpv4.lease_time);
        printk("Subnet: %s\n",
                net_addr_ntop(AF_INET,
                                        &iface->config.ip.ipv4->netmask,
                                        buf, sizeof(buf)));
        printk("Router: %s\n",
                net_addr_ntop(AF_INET,
                                                &iface->config.ip.ipv4->gw,
                                                buf, sizeof(buf)));
}


void init_network(void)
{
        net_mgmt_init_event_callback(&dhcp_cb, handler_cb,
                                     NET_EVENT_IPV4_DHCP_BOUND);

        net_mgmt_add_event_callback(&dhcp_cb);

        iface = net_if_get_default();
        if (!iface) {
                printk("network interface not available\n");
                return;
        }

	printk("DHCP start...\r\n");
        net_dhcpv4_start(iface);
}

struct net_if *get_network_interface(void) {
	return iface;
}

bool is_network_ready(void) {
	return network_ready;
}
