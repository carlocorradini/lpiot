#include "sensor.h"

#include "config/config.h"
#include "etc/etc.h"
#include "logger/logger.h"
#include "node/node.h"

/**
 * @brief Last (current) sensed value.
 */
static uint32_t sensor_value;

/**
 * @brief Threshold after which an event is triggered.
 */
static uint32_t sensor_threshold;

/**
 * @brief Timer to periodically sense new value.
 */
static struct ctimer sensor_timer;

/**
 * @brief Last command message received.
 */
static struct {
  /* Even sequence number */
  uint16_t event_seqn;
  /* Event source */
  linkaddr_t event_source;
  /* Command type. */
  enum command_type_t type;
  /* Command threshold. */
  uint32_t threshold;
} last_command;

/**
 * @brief Periodic function to update the sensed value (and trigger events).
 *
 * @param ignored
 */
static void sensor_timer_cb(void *ignored);

/**
 * @brief Command reception callback.
 * Notifies the sensor/actuator of a command from the controller.
 *
 * @param event_seqn Event sequence number.
 * @param event_source Event source node.
 * @param command Command type.
 * @param threshold New threshold.
 */
static void command_cb(uint16_t event_seqn, const linkaddr_t *event_source,
                       enum command_type_t command, uint32_t threshold);

/**
 * @brief Callbacks.
 */
static const struct etc_callbacks_t etc_cb = {
    .event_cb = NULL, .collect_cb = NULL, .command_cb = command_cb};

void sensor_init(size_t index) {
  /* Data */
  sensor_value = SENSOR_INITIAL_VALUE * index;
  sensor_threshold = CONTROLLER_MAX_DIFF;
  /* Last command */
  last_command.type = COMMAND_TYPE_NONE;
  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
  /* Open ETC connection */
  etc_open(CONNECTION_CHANNEL, &etc_cb);
}

void sensor_terminate(void) {
  /* Data */
  sensor_value = 0;
  sensor_threshold = 0;
  /* Last command */
  last_command.type = COMMAND_TYPE_NONE;
  /* Timer */
  ctimer_stop(&sensor_timer);
  /* Close ETC connection */
  etc_close();
}

static void sensor_timer_cb(void *ignored) {
  /* Increase sensor value */
  sensor_value += SENSOR_UPDATE_INCREMENT;

  /* Update ETC sensor data */
  etc_update(sensor_value, sensor_threshold);

  LOG_INFO("Reading { value: %lu, threshold: %lu }", sensor_value,
           sensor_threshold);
#ifdef STATS
  printf("Reading (%lu, %lu)\n", sensor_value, sensor_threshold);
#endif

  /* Check threshold */
  if (sensor_value > sensor_threshold) {
    /* Trigger */
    if (!etc_trigger(sensor_value, sensor_threshold)) {
      /* Fail */
      LOG_WARN("Trigger is suppressed");
    } else {
      /* Success */
      const struct etc_event_t *event = etc_get_current_event();
      LOG_INFO("Trigger { seqn: %u, source: %02x:%02x }", event->seqn,
               event->source.u8[0], event->source.u8[1]);
#ifdef STATS
      printf("TRIGGER [%02x:%02x, %u]\n", event->source.u8[0],
             event->source.u8[1], event->seqn);
#endif
    }
  }

  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
}

static void command_cb(uint16_t event_seqn, const linkaddr_t *event_source,
                       enum command_type_t command, uint32_t threshold) {
  /* Check if received duplicated command */
  if (last_command.event_seqn == event_seqn &&
      linkaddr_cmp(&last_command.event_source, event_source) &&
      last_command.type == command && last_command.threshold == threshold) {
    LOG_WARN(
        "Duplicated command: "
        "{ command: %d, threshold: %lu, "
        "event_seqn: %u, event_source: %02x:%02x }: ",
        command, threshold, event_seqn, event_source->u8[0],
        event_source->u8[1]);
    return;
  }

  LOG_INFO(
      "Command: "
      "{ command: %d, threshold: %lu, "
      "event_seqn: %u, event_source: %02x:%02x }: ",
      command, threshold, event_seqn, event_source->u8[0], event_source->u8[1]);
#ifdef STATS
  printf("ACTUATION [%02x:%02x, %u] %02x:%02x\n", event_source->u8[0],
         event_source->u8[1], event_seqn, linkaddr_node_addr.u8[0],
         linkaddr_node_addr.u8[1]);
#endif

  /* Actuate */
  switch (command) {
    case COMMAND_TYPE_RESET: {
      LOG_INFO(
          "Command RESET: From "
          "{ value: %lu, threshold: %lu } to "
          "{ value: %lu, threshold: %lu }",
          sensor_value, sensor_threshold, (uint32_t)0, threshold);
      sensor_value = 0;
      sensor_threshold = threshold;
      break;
    }
    case COMMAND_TYPE_THRESHOLD: {
      LOG_INFO(
          "Command THRESHOLD: From "
          "{ value: %lu, threshold: %lu } to "
          "{ value: %lu, threshold: %lu }",
          sensor_value, sensor_threshold, sensor_value, threshold);
      sensor_threshold = threshold;
      break;
    }
    case COMMAND_TYPE_NONE: {
      LOG_INFO("Command NONE: Ignoring...");
      break;
    }
  }

  /* Update last command */
  last_command.event_seqn = event_seqn;
  linkaddr_copy(&last_command.event_source, event_source);
  last_command.type == command;
  last_command.threshold == threshold;
}
