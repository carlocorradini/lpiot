#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <net/linkaddr.h>
#include <net/packetbuf.h>
#include <sys/types.h>

#include "node/node.h"

/**
 * @brief Connection object.
 */
struct connection_t {
  /**
   * @brief Parent node address.
   */
  linkaddr_t parent_node;
  /**
   * @brief Sequence number.
   */
  uint16_t seqn;
  /**
   * @brief Hop number.
   */
  uint16_t hopn;
  /**
   * @brief RSSI parent node.
   */
  packetbuf_attr_t rssi;
};

/**
 * @brief Best connection pointer.
 * Represents the best connection for communication
 * in the architecture.
 */
extern const struct connection_t* const best_conn;

/**
 * @brief Initialize node connection(s).
 *
 * @param node_role Node role.
 * @param channel Channel(s) on which the connection will operate.
 */
void connection_init(node_role_t node_role, uint16_t channel);

#endif