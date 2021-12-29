#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include <net/rime/broadcast.h>
#include <sys/types.h>

#include "connection/connection.h"
#include "node/node.h"

/**
 * @brief Initialize beacon connection.
 *
 * @param best_conn Best connection pointer.
 * @param bc_conn Broadcast connection (opened).
 * @param role Node role.
 */
void beacon_init(const struct connection_t *best_conn,
                 struct broadcast_conn *bc_conn, node_role_t role);

/**
 * @brief Broadcast receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
void beacon_bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender);

#endif