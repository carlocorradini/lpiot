#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include <net/rime/broadcast.h>
#include <sys/types.h>

#include "connection/connection.h"
#include "node/node.h"

/**
 * @brief Initialize beacon connection.
 *
 * @param bc_conn Broadcast connection (opened).
 * @param role Node role.
 */
void beacon_init(struct broadcast_conn *bc_conn, node_role_t role);

/**
 * @brief Broadcast receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
void beacon_bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender);

/**
 * @brief Return beacon connection information.
 *
 * @return const connection_info_t Connection information.
 */
const struct connection_info_t beacon_connection_info(void);

#endif