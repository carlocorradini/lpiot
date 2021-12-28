#ifndef _NODE_CONTROLLER_H_
#define _NODE_CONTROLLER_H_

#include "etc/etc.h"

/**
 * @brief Initialize Controller node.
 *
 * @param conn Pointer to an ETC connection object.
 */
void controller_init(struct etc_conn_t *conn);

#endif