#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <net/linkaddr.h>

/**
 * @brief Connection information.
 */
struct connection_info_t {
  /**
   * @brief Parent node address.
   */
  const linkaddr_t *parent_node;
};

#endif