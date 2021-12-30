#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <lib/random.h>
#include <net/linkaddr.h>
#include <sys/clock.h>

#include "logger/logger.h"

/* --- LOGGER --- */
/**
 * @brief Logger level.
 */
#ifndef DEBUG
#define LOGGER_LEVEL LOG_LEVEL_INFO
#else
#define LOGGER_LEVEL LOG_LEVEL_DEBUG
#endif

/* --- CONNECTION --- */
/**
 * @brief Maximum number of connections to store
 */
#define CONNECTION_BEACON_MAX_CONNECTIONS (3)

/* --- ETC --- */
/* FIXME Rimuovi etc please */

/**
 * @brief Starting channel (ETC may use multiple channels).
 */
#define ETC_FIRST_CHANNEL (0xAA)

/**
 * @brief Interval to (re)create the connections tree.
 * Valid only if the node is a Controller.
 */
#define ETC_BEACON_INTERVAL (CLOCK_SECOND * 30)

/**
 * @brief Time to wait before sending a beacon message.
 */
#define ETC_BEACON_FORWARD_DELAY (random_rand() % CLOCK_SECOND)

/**
 * @brief Time to wait before sending an event message.
 */
#define ETC_EVENT_FORWARD_DELAY (random_rand() % (CLOCK_SECOND / 10))

/**
 * @brief Time to wait before sending a collect message.
 */
#define ETC_COLLECT_START_DELAY \
  (CLOCK_SECOND * 3 + random_rand() % (CLOCK_SECOND * 2))

/**
 * @brief Timeout for new event generation.
 */
#define SUPPRESSION_TIMEOUT_NEW (CLOCK_SECOND * 12)

/**
 * @brief Timeout for event repropagation.
 */
#define SUPPRESSION_TIMEOUT_PROP (SUPPRESSION_TIMEOUT_NEW - CLOCK_SECOND / 2)

/**
 * @brief Timeout after a command to disable suppression.
 */
#define ETC_SUPPRESSION_TIMEOUT_END (CLOCK_SECOND / 2)

/**
 * @brief RSSI threshold.
 */
#define ETC_RSSI_THRESHOLD (-95)

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