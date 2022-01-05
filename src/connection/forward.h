#ifndef _CONNECTION_FORWARD_H_
#define _CONNECTION_FORWARD_H_

#include <net/linkaddr.h>

#include "config/config.h"

/**
 * @brief Forward table entry.
 * Defines how a message to a Sensor node should be forwarded.
 * Note that there could be no forwarding rule available.
 */
struct forward_t {
  /* Sensor node address (receiver). */
  linkaddr_t sensor;
  /* Forwarding node addresses (next-hop). */
  linkaddr_t hops[CONNECTION_FORWARD_MAX_SIZE];
};

/**
 * @brief Initialize forward structure.
 */
void forward_init(void);

/**
 * @brief Terminate forward structure.
 */
void forward_terminate(void);

/**
 * @brief Find a forward entry by sensor address.
 *
 * @param sensor Sensor address.
 * @return Forward entry.
 */
struct forward_t* forward_find(const linkaddr_t* sensor);

/**
 * @brief Add a next hop to reach sensors.
 *
 * @param sensor Sensor address.
 * @param next_hop Next hop adress.
 */
void forward_add(const linkaddr_t* sensor, const linkaddr_t* next_hop);

#endif
