#include "node.h"

#include <net/linkaddr.h>
#include <sys/types.h>

#include "config/config.h"

/**
 * @brief Node role cache.
 * A node can have only one role.
 * To not recalculate it cache it.
 */
static enum node_role_t node_role_cache = NODE_ROLE_UNKNOWN;

/**
 * @brief String representation of node roles.
 */
static const char const* node_role_strings[] = {"UNKNOWN", "CONTROLLER",
                                                "SENSOR/ACTUATOR", "FORWARDER"};

enum node_role_t node_get_role(void) {
  size_t i;

  /* Check in cache */
  if (node_role_cache != NODE_ROLE_UNKNOWN) return node_role_cache;

  /* Controller */
  if (linkaddr_cmp(&CONTROLLER, &linkaddr_node_addr))
    return node_role_cache = NODE_ROLE_CONTROLLER;

  /* Sensor/Actuator */
  for (i = 0; i < NUM_SENSORS; ++i) {
    if (linkaddr_cmp(&SENSORS[i], &linkaddr_node_addr))
      return node_role_cache = NODE_ROLE_SENSOR_ACTUATOR;
  }

  /* Forwarder */
  return node_role_cache = NODE_ROLE_FORWARDER;
}

const char const* node_get_role_name(void) {
  return node_role_strings[node_get_role()];
}
