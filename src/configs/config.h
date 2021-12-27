#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <core/net/linkaddr.h>
#include <core/sys/clock.h>
#include <lib/random.h>

/* --- ETC --- */
/**
 * @brief Starting channel (ETC may use multiple channels).
 */
#define ETC_FIRST_CHANNEL (0xAA)

/* --- CONTROLLER --- */
/**
 * @brief Controller address.
 */
extern const linkaddr_t CONTROLLER;

/**
 * @brief Maximum sensor value difference.
 */
#define CONTROLLER_MAX_DIFF (10000)

/**
 * @brief Maximum sensor threshold.
 */
#define CONTROLLER_MAX_THRESHOLD (50000)

/**
 * @brief Critical difference.
 */
#define CONTROLLER_CRITICAL_DIFF (15000)

/**
 * @brief Interval to collect sensor's values.
 */
#define CONTROLLER_COLLECT_WAIT (CLOCK_SECOND * 10)

/* --- SENSOR --- */
/**
 * @brief Total number of Sensor nodes available.
 */
#define NUM_SENSORS (5)

/**
 * @brief Sensor's addresses.
 */
extern const linkaddr_t SENSORS[NUM_SENSORS];

/**
 * @brief Interval to sense the value.
 */
#define SENSOR_UPDATE_INTERVAL (CLOCK_SECOND * 7)

/**
 * @brief Random increment to add to the old sensed value.
 */
#define SENSOR_UPDATE_INCREMENT (random_rand() % 300)

/**
 * @brief Initial sensed value step.
 */
#define SENSOR_INITIAL_VALUE (1000)

#endif