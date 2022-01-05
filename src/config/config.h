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
#ifndef STATS
#define LOGGER_LEVEL LOG_LEVEL_INFO
#else
#define LOGGER_LEVEL LOG_LEVEL_DISABLED
#endif

/* --- ETC --- */
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
 * @brief New event generation suppression time.
 */
#define ETC_SUPPRESSION_EVENT_NEW (CLOCK_SECOND * 12)

/**
 * @brief Event propagation suppression time.
 */
#define ETC_SUPPRESSION_EVENT_PROPAGATION \
  (ETC_SUPPRESSION_EVENT_NEW - CLOCK_SECOND / 2)

/**
 * @brief Time to wait to disable suppression propagation after received a
 * command message.
 */
#define ETC_SUPPRESSION_EVENT_PROPAGATION_END (CLOCK_SECOND / 2)

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
 * @brief Time to wait before analyzing the Sensor readings.
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

/* --- CONNECTION --- */
/**
 * @brief Channel(s) on which the connection will operate.
 */
#define CONNECTION_CHANNEL (0xAA)

/**
 * @brief RSSI threshold.
 */
#define CONNECTION_RSSI_THRESHOLD (-95)

/**
 * @brief Maximum number of connections to store
 */
#define CONNECTION_BEACON_MAX_CONNECTIONS (3)

/**
 * @brief Interval to (re)create the connections tree.
 * Valid only if the node is a Controller.
 */
#define CONNECTION_BEACON_INTERVAL (CLOCK_SECOND * 30)

/**
 * @brief Time to wait before sending a beacon message.
 */
#define CONNECTION_BEACON_FORWARD_DELAY (random_rand() % CLOCK_SECOND)

/**
 * @brief Unicast buffer size.
 * The maximum number of unicast messages that the buffer could store.
 * A good value is the number of Sensor nodes available.
 */
#define CONNECTION_UC_BUFFER_SIZE (NUM_SENSORS)

/**
 * @brief Maximum number of send attempts for a packet in the buffer.
 */
#define CONNECTION_UC_BUFFER_MAX_SEND (1)

/**
 * @brief Maximum number of hops in forwarding structure.
 */
#define CONNECTION_FORWARD_MAX_SIZE (3)

#endif
