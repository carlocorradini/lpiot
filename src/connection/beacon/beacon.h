#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include <net/linkaddr.h>
#include <sys/types.h>

#include "node/node.h"

/**
 * @brief Initialize beacon connection.
 *
 * @param role Node role.
 * @param channel Channel on which the connection will operate.
 */
void beacon_init(node_role_t role, uint16_t channel);

/**
 * @brief Return beacon connection information.
 *
 * @return const struct linkaddr_t* Parent node address.
 */
const linkaddr_t* beacon_connection_info(void);

#endif