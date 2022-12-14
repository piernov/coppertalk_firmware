#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

void init_network(void);
struct net_if *get_network_interface(void);
bool is_network_ready(void);

#endif
