#ifndef _NODE_FORWARDER_H_
#define _NODE_FORWARDER_H_

#include "etc/etc.h"

/**
 * @brief Initialize Forwarder node.
 *
 * @param conn Pointer to an ETC connection object.
 */
void forwarder_init(struct etc_conn_t *conn);

#endif