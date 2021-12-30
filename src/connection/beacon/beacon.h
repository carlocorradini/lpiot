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
 * @param role Node role.
 */
void beacon_init(const struct connection_t *best_conn, node_role_t role);

/**
 * @brief Broadcast receive callback.
 *
 * @param header Broadcast header.
 * @param sender Address of the sender node.
 */
void beacon_recv_cb(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender);

#endif