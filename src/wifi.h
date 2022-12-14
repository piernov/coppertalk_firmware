#ifndef WIFI_H
#define WIFI_H

#include <stdbool.h>

void init_at_wifi(void);
struct net_if *get_wifi_intf(void);
bool is_wifi_ready(void);

#endif
