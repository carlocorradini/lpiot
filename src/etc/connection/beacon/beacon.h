#ifndef _CONNECTION_BEACON__H_
#define _CONNECTION_BEACON__H_

#include "etc/etc.h"

/**
 * @brief Initialize beacon connection.
 *
 * @param conn Pointer to a ETC connection object.
 * @param channel Channel on which the connection will operate.
 */
void beacon_init(struct etc_conn_t *conn, uint16_t channel);

#endif