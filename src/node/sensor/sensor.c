#include "sensor.h"

#include "config/config.h"
#include "logger/logger.h"

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
 * @brief Periodic function to update the sensed value (and trigger events).
 *
 * @param ptr An opaque pointer that will be supplied as an argument to the
 * callback function.
 */
static void sensor_timer_cb(void *ptr);

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
                       command_type_t command, uint32_t threshold);

/**
 * @brief Connection.
 */
static struct etc_conn_t *etc_conn = NULL;

/**
 * @brief Callbacks.
 */
static const struct etc_callbacks_t cb = {
    .receive_cb = NULL, .event_cb = NULL, .command_cb = command_cb};

void sensor_init(struct etc_conn_t *conn, size_t sensor_index) {
  etc_conn = conn;
  sensor_value = SENSOR_INITIAL_VALUE * sensor_index;
  sensor_threshold = CONTROLLER_MAX_DIFF;

  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);

  /* Open connection */
  etc_open(etc_conn, ETC_FIRST_CHANNEL, NODE_ROLE_SENSOR_ACTUATOR, &cb, SENSORS,
           NUM_SENSORS);
}

static void sensor_timer_cb(void *ptr) {
  /* Increase sensor value */
  sensor_value += SENSOR_UPDATE_INCREMENT;

  etc_update(sensor_value, sensor_threshold);
  LOG_INFO("Reading { value: %lu, threshold: %lu }", sensor_value,
           sensor_threshold);

  if (sensor_value > sensor_threshold) {
    int ret = etc_trigger(etc_conn, sensor_value, sensor_threshold);
    if (ret) {
      LOG_INFO("Trigger [%02x:%02x, %u]", etc_conn->event_source.u8[0],
               etc_conn->event_source.u8[1], etc_conn->event_seqn);
    }
  }

  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
}

static void command_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                       command_type_t command, uint32_t threshold) {
  LOG_INFO("Actuation [%02x:%02x, %u] %02x:%02x", event_source->u8[0],
           event_source->u8[1], event_seqn, linkaddr_node_addr.u8[0],
           linkaddr_node_addr.u8[1]);

  switch (command) {
    case COMMAND_TYPE_NONE: {
      LOG_INFO("Received COMMAND_TYPE_NONE. Ignoring it...");
      break;
    }
    case COMMAND_TYPE_RESET: {
      LOG_INFO(
          "Received COMMAND_TYPE_RESET. From "
          "{ value: %lu, threshold: %lu } to "
          "{ value: %lu, threshold: %lu }",
          sensor_value, sensor_threshold, (uint32_t)0,
          (uint32_t)CONTROLLER_MAX_DIFF);
      sensor_value = 0;
      /* TODO Magari usa il threshold se passato da controller */
      sensor_threshold = CONTROLLER_MAX_DIFF;
      break;
    }
    case COMMAND_TYPE_THRESHOLD: {
      LOG_INFO(
          "Received COMMAND_TYPE_THRESHOLD. From "
          "{ value: %lu, threshold: %lu } to "
          "{ value: %lu, threshold: %lu }",
          sensor_value, sensor_threshold, sensor_value, threshold);
      sensor_threshold = threshold;
      break;
    }
  }
}