#include "controller.h"

#include <stdio.h>

#include "configs/config.h"

/**
 * @brief Sensor reading.
 */
typedef struct {
  linkaddr_t addr;
  uint16_t seqn;
  bool reading_available;
  uint32_t value;
  uint32_t threshold;
  command_type_t command;
} sensor_reading_t;

/**
 * @brief Sensor readings.
 * Save the last reading of i th sensor.
 */
static sensor_reading_t sensor_readings[NUM_SENSORS];

/**
 * @brief Total number of readings from sensors.
 */
static uint8_t num_sensor_readings;

/**
 * @brief Data collection reception callback.
 * The controller sets this callback to store the readings of all sensors.
 * When all readings have been collected, the controller can send commands.
 * You may send commands earlier if some data is missing after a timeout,
 * running actuation logic on the acquired data.
 *
 * @param event_source Address of the sensor that generated the event.
 * @param event_seqn Event sequence number.
 * @param source Address of the source sensor.
 * @param value Sensor's value.
 * @param threshold Sensor's threshold.
 */
static void receive_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                       const linkaddr_t *source, uint32_t value,
                       uint32_t threshold);

/**
 * @brief Event detection callback.
 * This callback notifies the controller of an ongoing event dissemination.
 * After this notification, the controller waits for sensor readings.
 * The event callback should come with the event_source (the address of the
 * sensor that generated the event) and the event_seqn (a growing sequence
 * number).
 * The logging, reporting source and sequence number, can be
 * matched with data collection logging to count how many packets,
 * associated to this event, were received.
 *
 * @param event_source Address of the sensor that generated the event.
 * @param event_seqn Event sequence number.
 */
static void event_cb(const linkaddr_t *event_source, uint16_t event_seqn);

/**
 * @brief Actuation logic.
 * The actuation logic to be called after sensor readings have been
 * collected. This functions checks for the steady state conditions and
 * assigns commands to all sensor/actuators that are violating them. Should
 * be called before actuation_commands(), which sends ACTUATION messages
 * based on the results of actuation_logic().
 */
static void actuation_logic(void);

/**
 * @brief Actuation commands.
 * Sends actuations for all sensors with a pending command.
 * actuation_commands() should be called after actuation_logic(), as
 * the logic sets the command for each sensor in their associated structure.
 */
static void actuation_commands(void);

/**
 * @brief Connection.
 */
static struct etc_conn_t *etc_conn = NULL;

/**
 * @brief Callbacks.
 */
static struct etc_callbacks_t cb = {
    .receive_cb = receive_cb, .event_cb = event_cb, .command_cb = NULL};

void controller_init(struct etc_conn_t *conn) {
  etc_conn = conn;

  /* Sensor structure */
  uint i;
  for (i = 0; i < NUM_SENSORS; ++i) {
    linkaddr_copy(&sensor_readings[i].addr, &SENSORS[i]);
    sensor_readings[i].reading_available = false;
    sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;
  }
  num_sensor_readings = 0;

  /* Open connection */
  etc_open(conn, ETC_FIRST_CHANNEL, NODE_ROLE_CONTROLLER, &cb, SENSORS,
           NUM_SENSORS);
}

static void receive_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                       const linkaddr_t *source, uint32_t value,
                       uint32_t threshold) {
  /* What if controller has not seen the event message for this collection?
   * Add proper logging! */

  /* Add sensor reading (careful with duplicates!) */

  /* Logging (based on the current event handled by the controller,
   * identified by the event_source and its sequence number);
   * in principle, this may not be the same event_source and event_seqn
   * in the callback, if the transmission was triggered by a
   * concurrent event. To match logs, the controller should
   * always use the same event_source and event_seqn for collection
   * and actuation */
  printf("COLLECT [%02x:%02x, %u] %02x:%02x (%lu, %lu)\n",
         etc_conn->event_source.u8[0], etc_conn->event_source.u8[1],
         etc_conn->event_seqn, source->u8[0], source->u8[1], value, threshold);

  /* If all data was collected, call actuation logic */
}

static void event_cb(const linkaddr_t *event_source, uint16_t event_seqn) {
  /* Check if the event is old and discard it in that case;
   * otherwise, update the current event being handled */

  /* Logging */
  printf("EVENT [%02x:%02x, %u]\n", etc_conn->event_source.u8[0],
         etc_conn->event_source.u8[1], etc_conn->event_seqn);

  /* Wait for sensor readings */
}

/**
 * @brief Actuation logic.
 * The actuation logic to be called after sensor readings have been
 * collected. This functions checks for the steady state conditions and
 * assigns commands to all sensor/actuators that are violating them. Should
 * be called before actuation_commands(), which sends ACTUATION messages
 * based on the results of actuation_logic().
 */
static void actuation_logic(void) {
  if (num_sensor_readings < 1) {
    printf("Controller: No data collected\n");
    return;
  }

  /* Debug: missing sensors */
  int i, j;
  for (i = 0; i < NUM_SENSORS; i++) {
    if (!sensor_readings[i].reading_available) {
      printf("Controller: Missing %02x:%02x data\n",
             sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1]);
    }
  }

  /* Search for nodes in need of actuation */
  bool restart_check = false;
  while (true) {
    restart_check = false;

    /* Find min */
    uint32_t value_min = 0;
    for (i = 0; i < NUM_SENSORS; i++) {
      if (sensor_readings[i].reading_available) {
        value_min = sensor_readings[i].value;
        break;
      }
    }
    for (; i < NUM_SENSORS; i++) {
      if (sensor_readings[i].reading_available) {
        value_min = MIN(value_min, sensor_readings[i].value);
      }
    }

    /* Check for any violation of the steady state condition,
     * and for sensors with outdated thresholds. */
    for (i = 0; i < NUM_SENSORS; i++) {
      for (j = 0; j < NUM_SENSORS; j++) {
        if (!sensor_readings[i].reading_available) continue;

        /* Check actuation command needed, if any;
         * case 1) the maximum difference is being exceeded;
         * case 2) the current (local) threshold of a node is being
         * exceeded.
         */
        if (sensor_readings[j].reading_available &&
            (sensor_readings[i].value >=
                 sensor_readings[j].value + CONTROLLER_MAX_DIFF ||
             sensor_readings[i].threshold > CONTROLLER_MAX_THRESHOLD)) {
          sensor_readings[i].command = COMMAND_TYPE_RESET;
          sensor_readings[i].value = 0;
          sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;

          printf("Controller: Reset %02x:%02x (%lu, %lu)\n",
                 sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1],
                 sensor_readings[i].value, sensor_readings[i].threshold);

          /* A value was changed, restart values check */
          restart_check = true;
        } else if (sensor_readings[i].value > sensor_readings[i].threshold) {
          sensor_readings[i].command = COMMAND_TYPE_THRESHOLD;
          sensor_readings[i].threshold += value_min;

          printf("Controller: Update threshold %02x:%02x (%lu, %lu)\n",
                 sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1],
                 sensor_readings[i].value, sensor_readings[i].threshold);

          /* A value was changed, restart values check */
          restart_check = true;
        }
      }
    }
    if (!restart_check) break;
  }
}

/**
 * @brief Actuation commands.
 * Sends actuations for all sensors with a pending command.
 * actuation_commands() should be called after actuation_logic(), as
 * the logic sets the command for each sensor in their associated structure.
 */
static void actuation_commands(void) {
  int i;
  for (i = 0; i < NUM_SENSORS; i++) {
    if (sensor_readings[i].command != COMMAND_TYPE_NONE) {
      etc_command(etc_conn, &sensor_readings[i].addr,
                  sensor_readings[i].command, sensor_readings[i].threshold);

      /* Logging (based on the current event, expressed by source seqn) */
      printf("COMMAND [%02x:%02x, %u] %02x:%02x\n",
             etc_conn->event_source.u8[0], etc_conn->event_source.u8[1],
             etc_conn->event_seqn, sensor_readings[i].addr.u8[0],
             sensor_readings[i].addr.u8[1]);
    }
  }
}
