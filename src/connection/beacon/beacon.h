#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include <sys/types.h>

#include "connection/connection.h"
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
 * @return const connection_info_t Connection information.
 */
const struct connection_info_t beacon_connection_info(void);

#endif