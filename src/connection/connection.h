#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <net/linkaddr.h>
#include <sys/types.h>

#include "node/node.h"

/**
 * @brief Connection information.
 */
struct connection_info_t {
  /**
   * @brief Parent node address.
   */
  const linkaddr_t *parent_node;
};

/**
 * @brief Initialize node connection(s).
 *
 * @param node_role Node role.
 * @param channel Channel(s) on which the connection will operate.
 */
void connection_init(node_role_t node_role, uint16_t channel);

#endif