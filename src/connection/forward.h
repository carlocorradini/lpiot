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
  /* Next hop address. */
  linkaddr_t hop;
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
 * @brief Add a new hop to reach the sensor.
 *
 * @param sensor Sensor address.
 * @param hop Next hop adress.
 */
void forward_add(const linkaddr_t* sensor, const linkaddr_t* hop);

/**
 * @brief Remove the hop of the sensor.
 *
 * @param sensor Sensor address.
 */
void forward_remove(const linkaddr_t* sensor);

/**
 * @brief Check if exists a hop for the sensor.
 *
 * @param sensor Sensor address.
 * @return true Hop available.
 * @return false Hop not available.
 */
bool forward_hop_available(const linkaddr_t* sensor);

#endif
