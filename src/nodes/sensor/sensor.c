#include "sensor.h"

#include <stdio.h>

#include "config/config.h"

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
static struct etc_callbacks_t cb = {
    .receive_cb = NULL, .event_cb = NULL, .command_cb = command_cb};

void sensor_init(struct etc_conn_t *conn, uint sensor_index) {
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
  printf("[SENSOR]: Reading { value: %lu, threshold: %lu }\n", sensor_value,
         sensor_threshold);

  if (sensor_value > sensor_threshold) {
    int ret = etc_trigger(etc_conn, sensor_value, sensor_threshold);
    if (ret) {
      printf("[SENSOR]: Trigger [%02x:%02x, %u]\n",
             etc_conn->event_source.u8[0], etc_conn->event_source.u8[1],
             etc_conn->event_seqn);
    }
  }

  /* Periodic update of the sensed value */
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
}

static void command_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                       command_type_t command, uint32_t threshold) {
  /* TODO */
  /* Logging (based on the source and sequence number in the command message
   * sent by the sink, to guarantee that command transmission and
   * actuation can be matched by the analysis scripts) */
  printf("ACTUATION [%02x:%02x, %u] %02x:%02x\n", event_source->u8[0],
         event_source->u8[1], event_seqn, linkaddr_node_addr.u8[0],
         linkaddr_node_addr.u8[1]);

  /* Execute commands */
}