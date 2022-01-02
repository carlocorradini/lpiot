#include "sensor.h"

#include "config/config.h"
#include "etc/etc.h"
#include "logger/logger.h"
#include "node/node.h"

/**
 * @brief Last (current) sensed value.
 */
static uint32_t value;

/**
 * @brief Threshold after which an event is triggered.
 */
static uint32_t threshold;

/**
 * @brief Timer to periodically sense new value.
 */
static struct ctimer sensor_timer;

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
 * @param event_source Event source node.
 * @param event_seqn Event sequence number.
 * @param command Command type.
 * @param threshold New threshold.
 */
static void command_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                       enum command_type_t command, uint32_t threshold);

/**
 * @brief Callbacks.
 */
static const struct etc_callbacks_t etc_cb = {
    .event_cb = NULL, .collect_cb = NULL, .command_cb = command_cb};

void sensor_init(size_t index) {
  value = SENSOR_INITIAL_VALUE * index;
  threshold = CONTROLLER_MAX_DIFF;

  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);

  /* Open ETC connection */
  etc_open(CONNECTION_CHANNEL, &etc_cb);
}

static void sensor_timer_cb(void *ignored) {
  /* Increase sensor value */
  value += SENSOR_UPDATE_INCREMENT;

  /* Update ETC sensor data */
  etc_update(value, threshold);

  LOG_INFO("Reading { value: %lu, threshold: %lu }", value, threshold);

  /* Check threshold */
  if (value > threshold) {
    /* Trigger */
    if (!etc_trigger(value, threshold)) {
      /* Fail */
      LOG_WARN("Trigger is suppressed");
    } else {
      /* Success */
      const struct etc_event_t *event = etc_get_current_event();
      LOG_INFO("Trigger [%02x:%02x, %u]", event->source.u8[0],
               event->source.u8[1], event->seqn);
    }
  }

  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
}

static void command_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                       enum command_type_t command, uint32_t threshold) {
  LOG_INFO("Actuation [%02x:%02x, %u] %02x:%02x", event_source->u8[0],
           event_source->u8[1], event_seqn, linkaddr_node_addr.u8[0],
           linkaddr_node_addr.u8[1]);

  switch (command) {
    case COMMAND_TYPE_NONE: {
      LOG_INFO("Received COMMAND_TYPE_NONE: Ignoring it...");
      break;
    }
    case COMMAND_TYPE_RESET: {
      LOG_INFO(
          "Received COMMAND_TYPE_RESET: From "
          "{ value: %lu, threshold: %lu } to "
          "{ value: %lu, threshold: %lu }",
          value, threshold, (uint32_t)0, (uint32_t)CONTROLLER_MAX_DIFF);
      value = 0;
      /* TODO Magari usa il threshold se passato da controller */
      threshold = CONTROLLER_MAX_DIFF;
      break;
    }
    case COMMAND_TYPE_THRESHOLD: {
      LOG_INFO(
          "Received COMMAND_TYPE_THRESHOLD: From "
          "{ value: %lu, threshold: %lu } to "
          "{ value: %lu, threshold: %lu }",
          value, threshold, value, threshold);
      threshold = threshold;
      break;
    }
  }
}
