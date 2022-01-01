#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include "connection/connection.h"

/**
 * @brief Initialize beacon operation(s).
 */
void beacon_init(void);

/**
 * @brief Terminate beacon operation(s).
 */
void beacon_terminate(void);

/**
 * @brief Return established connection.
 * Connection is always defined but check parent_node to be not
 * linkaddr_null to know if the connection is valid.
 *
 * @return Established connection.
 */
const struct connection_t *beacon_get_conn(void);

/**
 * @brief Broadcast receive callback.
 *
 * @param header Broadcast header.
 * @param sender Address of the sender node.
 */
void beacon_recv_cb(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender);

/**
 * @brief Invalidate current connection.
 * After invalidation the new connection is the next available backup
 * connection.
 */
void beacon_invalidate_connection(void);

#endif
