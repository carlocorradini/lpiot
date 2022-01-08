#ifndef _CONNECTION_FORWARD_H_
#define _CONNECTION_FORWARD_H_

#include <net/linkaddr.h>

#include "config/config.h"

/**
 * @brief Hop forward structure.
 */
struct forward_hop_t {
  /* Address of the hop. */
  linkaddr_t address;
  /* Hop distance. */
  uint8_t distance;
};

/**
 * @brief Forward table entry.
 * Defines how a message to a Sensor node should be forwarded.
 * Note that there could be no forwarding rule available.
 */
struct forward_t {
  /* Sensor node address (receiver). */
  linkaddr_t sensor;
  /* Forwarding nodes (next-hop). */
  struct forward_hop_t hops[CONNECTION_FORWARD_MAX_SIZE];
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
 * @param hop_address Hop address.
 * @param hop_distance Hop distance.
 */
void forward_add(const linkaddr_t* sensor, const linkaddr_t* hop_address,
                 uint8_t hop_distance);

/**
 * @brief Remove the first available hop of the sensor.
 *
 * @param sensor Sensor address.
 */
void forward_remove(const linkaddr_t* sensor);

/**
 * @brief Remove a specific hop of the sensor.
 *
 * @param sensor Sensor address.
 * @param hop_address Hop address.
 */
void forward_remove_hop(const linkaddr_t* sensor,
                        const linkaddr_t* hop_address);

/**
 * @brief Return the number of available hops of the sensor node.
 *
 * @param sensor Sensor address.
 * @return Number of available hops.
 */
size_t forward_hops_length(const linkaddr_t* sensor);

#endif
