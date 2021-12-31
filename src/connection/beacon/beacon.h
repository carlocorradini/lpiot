#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include "connection/connection.h"

/**
 * @brief Initialize beacon operation(s).
 *
 * @param best_conn Best connection pointer.
 */
void beacon_init(const struct connection_t *best_conn);

/**
 * @brief Terminate beacon operation(s).
 */
void beacon_terminate(void);

/**
 * @brief Broadcast receive callback.
 *
 * @param header Broadcast header.
 * @param sender Address of the sender node.
 */
void beacon_recv_cb(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender);

#endif
