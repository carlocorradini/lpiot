#include "controller.h"

#include <sys/cc.h>

#include "config/config.h"
#include "etc/etc.h"
#include "logger/logger.h"

/**
 * @brief Sensor reading.
 */
struct sensor_reading_t {
  /* Address of the sensor/actuator node. */
  linkaddr_t address;
  /* Event sequence number. */
  uint16_t seqn;
  /* Sensor value. */
  uint32_t value;
  /* Sensor threshold. */
  uint32_t threshold;
  /* Flag if data is available. */
  bool reading_available;
  /* Command to send. */
  enum command_type_t command;
};

/**
 * @brief Sensor readings.
 * Save the last reading of i th sensor.
 */
static struct sensor_reading_t sensor_readings[NUM_SENSORS];

/**
 * @brief Total number of readings from sensors.
 */
static uint8_t num_sensor_readings;

/**
 * @brief Timer to wait before analyzing received sensor readings.
 */
static struct ctimer collect_timer;

/**
 * @brief Event detection callback.
 * Notifies of an ongoing event dissemination.
 * After this notification, the controller waits for sensor readings.
 *
 * @param event_seqn Event sequence number.
 * @param event_source Address of the sensor that generated the event.
 */
static void event_cb(uint16_t event_seqn, const linkaddr_t *event_source);

/**
 * @brief Data collection reception callback.
 * Store the readings of all sensors.
 * When all readings have been collected, the controller can send commands.
 *
 * @param event_seqn Event sequence number.
 * @param event_source Address of the sensor that generated the event.
 * @param sender Address of the sensor node.
 * @param value Sensor value.
 * @param threshold Sensor threshold.
 */
static void collect_cb(uint16_t event_seqn, const linkaddr_t *event_source,
                       const linkaddr_t *sender, uint32_t value,
                       uint32_t threshold);

/**
 * @brief Collect timer callback.
 *
 * @param ignored
 */
static void collect_timer_cb(void *ignored);

/**
 * @brief Actuation logic.
 * Actuation logic to be called after sensor readings have been
 * collected.
 * Checks for the steady state conditions and assigns commands to all
 * sensor(s)/actuator(s) that are violating them.
 */
static void actuation_logic(void);

/**
 * @brief Actuation commands.
 * Send command to sensor(s)/actuator(s) that needs actuation.
 */
static void actuation_commands(void);

/**
 * @brief Callbacks.
 */
static const struct etc_callbacks_t cb = {
    .event_cb = event_cb, .collect_cb = collect_cb, .command_cb = NULL};

void controller_init(void) {
  size_t i;

  /* Initialize sensor readings structure */
  for (i = 0; i < NUM_SENSORS; ++i) {
    linkaddr_copy(&sensor_readings[i].address, &SENSORS[i]);
    sensor_readings[i].seqn = 0;
    sensor_readings[i].value = 0;
    sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;
    sensor_readings[i].reading_available = false;
    sensor_readings[i].command = COMMAND_TYPE_NONE;
  }
  num_sensor_readings = 0;

  /* Open ETC connection */
  etc_open(CONNECTION_CHANNEL, &cb);
}

static void event_cb(uint16_t event_seqn, const linkaddr_t *event_source) {
  /* TODO Maybe ignorare se il timer non e' scaduto */
  struct sensor_reading_t *sensor_reading;
  size_t i;

  /* Find sensor reading */
  for (i = 0; i < NUM_SENSORS; ++i) {
    sensor_reading = &sensor_readings[i];
    if (linkaddr_cmp(event_source, &sensor_reading->address)) break; /* Found */
  }

  /* Check if event source is known */
  if (i >= NUM_SENSORS) {
    LOG_WARN("Event has unknown source: %02x:%02x", event_source->u8[0],
             event_source->u8[1]);
    return;
  }

  /* Check if event is old */
  if (event_seqn != 0 && event_seqn <= sensor_reading->seqn) {
    LOG_WARN(
        "Discarding event with source %02x:%02x because last reading of seqn "
        "%u >= %u received",
        event_source->u8[0], event_source->u8[1], sensor_reading->seqn,
        event_seqn);
    return;
  }

  /* Save event seqn */
  /* TODO not sure */
  sensor_reading->seqn = event_seqn;

  /* Clean possible old junk */
  num_sensor_readings = 0;
  for (i = 0; i < NUM_SENSORS; ++i) {
    sensor_readings[i].reading_available = false;
    sensor_readings[i].command = COMMAND_TYPE_NONE;
  }

  LOG_INFO(
      "Handling event: "
      "{ seqn: %u, source: %02x:%02x}",
      event_seqn, event_source->u8[0], event_source->u8[1]);

  /* Schedule sensor readings analysis */
  ctimer_set(&collect_timer, CONTROLLER_COLLECT_WAIT, collect_timer_cb, NULL);
}

static void collect_cb(uint16_t event_seqn, const linkaddr_t *event_source,
                       const linkaddr_t *sender, uint32_t value,
                       uint32_t threshold) {
  const struct etc_event_t *event = etc_get_current_event();
  struct sensor_reading_t *sensor_reading = NULL;
  struct sensor_reading_t *event_sensor_reading = NULL;
  size_t i;

  /* Find sensor reading(s) */
  for (i = 0; i < NUM_SENSORS; ++i) {
    /* Sensor reading */
    if (sensor_reading == NULL)
      sensor_reading = (linkaddr_cmp(sender, &sensor_readings[i].address)
                            ? &sensor_readings[i]
                            : NULL);
    /* Event sensor reading */
    if (event_sensor_reading == NULL)
      event_sensor_reading =
          (linkaddr_cmp(event_source, &sensor_readings[i].address)
               ? &sensor_readings[i]
               : NULL);
    /* Found */
    if (sensor_reading != NULL && event_sensor_reading != NULL) break;
  }

  /* Check if sender is known */
  if (sensor_reading == NULL) {
    LOG_WARN("Collect has unknown sender: %02x:%02x", sender->u8[0],
             sender->u8[1]);
    return;
  }
  /* Check if event source is known */
  if (event_sensor_reading == NULL) {
    LOG_WARN("Collect has unknown event source: %02x:%02x", event_source->u8[0],
             event_source->u8[1]);
    return;
  }

  /* Check if duplicate */
  if (sensor_reading->reading_available) {
    LOG_WARN("Collect from sensor %02x:%02x already received", sender->u8[0],
             sender->u8[1]);
    return;
  }

  /* Check if collect's event is currently handled */
  if (event_seqn != event->seqn ||
      !linkaddr_cmp(event_source, &event->source)) {
    LOG_WARN(
        "Collect event { seqn: %u, source: %02x:%02x } is not currently "
        "handled event { seqn: %u, source: %02x:%02x }",
        event_seqn, event_source->u8[0], event_source->u8[1], event->seqn,
        event->source.u8[0], event->source.u8[1]);
    return;
  }
  /* Check if collect's event is saved */
  if (event_seqn != event_sensor_reading->seqn ||
      !linkaddr_cmp(event_source, &event_sensor_reading->address)) {
    LOG_WARN(
        "Collect event { seqn: %u, source: %02x:%02x } "
        "is not saved { seqn: %u, source: %02x:%02x }",
        event_seqn, event_source->u8[0], event_source->u8[1],
        event_sensor_reading->seqn, event_sensor_reading->address.u8[0],
        event_sensor_reading->address.u8[1]);
    return;
  }

  /* Check if collect's event is old done in event_cb and checked thanks to
   * event-sensor_reading */

  /* Save reading */
  sensor_reading->value = value;
  sensor_reading->threshold = threshold;
  sensor_reading->reading_available = true;
  sensor_reading->command = COMMAND_TYPE_NONE;

  LOG_INFO(
      "Collect from sensor %02x:%02x of event { seqn: %u, source: %02x:%02x }: "
      "{ value: %lu, threshold: %lu }",
      sender->u8[0], sender->u8[1], event->seqn, event->source.u8[0],
      event->source.u8[1], value, threshold);

  /* Increase sensor readings counter */
  num_sensor_readings += 1;

  if (num_sensor_readings >= NUM_SENSORS) {
    /* Stop collect timer */
    ctimer_stop(&collect_timer);
    /* Trigger collect timer manually */
    collect_timer_cb(NULL);
  }
}

static void collect_timer_cb(void *ignored) {
  /* All sensors data are collected
   * or timer expired */

  /* Actuate */
  actuation_logic();
  /* Send command(s) */
  actuation_commands();
}

static void actuation_logic(void) {
  size_t i, j;
  bool restart_check = false;

  /* Check at least 1 sensor data collected */
  if (num_sensor_readings <= 0) {
    LOG_WARN("Could not actuate due to no data collected");
    return;
  }

  LOG_INFO("Collected data from %u/%u sensors", num_sensor_readings,
           NUM_SENSORS);

  /* Print sensor readings */
  for (i = 0; i < NUM_SENSORS; ++i) {
    if (!sensor_readings[i].reading_available) {
      LOG_WARN("Sensor %02x:%02x: { }", sensor_readings[i].address.u8[0],
               sensor_readings[i].address.u8[1]);
    } else {
      LOG_INFO("Sensor %02x:%02x: { seqn: %u, value: %lu, threshold: %lu } %s",
               sensor_readings[i].address.u8[0],
               sensor_readings[i].address.u8[1], sensor_readings[i].seqn,
               sensor_readings[i].value, sensor_readings[i].threshold,
               sensor_readings[i].value >= sensor_readings[i].threshold ? "!!!"
                                                                        : "");
    }
  }

  /* Search for nodes in need of actuation */
  while (true) {
    restart_check = false;
    uint32_t value_min = UINT32_MAX;

    /* Find min */
    for (i = 0; i < NUM_SENSORS; ++i) {
      if (sensor_readings[i].reading_available) {
        value_min = MIN(value_min, sensor_readings[i].value);
      }
    }

    /* Check for any violation of the steady state condition, and for sensors
     * with outdated thresholds */
    for (i = 0; i < NUM_SENSORS; ++i) {
      for (j = 0; j < NUM_SENSORS; ++j) {
        if (!sensor_readings[i].reading_available) continue;

        /* Check actuation command needed, if any:
         * case 1) The maximum difference is being exceeded
         * case 2) The current (local) threshold of a node is being exceeded
         */
        if (sensor_readings[j].reading_available &&
            (sensor_readings[i].value >=
                 sensor_readings[j].value + CONTROLLER_MAX_DIFF ||
             sensor_readings[i].threshold > CONTROLLER_MAX_THRESHOLD)) {
          /* Case 1: RESET */
          sensor_readings[i].command = COMMAND_TYPE_RESET;
          sensor_readings[i].value = 0;
          sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;

          LOG_DEBUG(
              "Actuation logic command RESET for sensor %02x:%02x: "
              "{ value: %lu, threshold: %lu }",
              sensor_readings[i].address.u8[0],
              sensor_readings[i].address.u8[1], sensor_readings[i].value,
              sensor_readings[i].threshold);

          /* Value changed */
          restart_check = true;
        } else if (sensor_readings[i].value > sensor_readings[i].threshold) {
          /* Case 2: THRESHOLD */
          sensor_readings[i].command = COMMAND_TYPE_THRESHOLD;
          sensor_readings[i].threshold += value_min;

          LOG_DEBUG(
              "Actuation logic command THRESHOLD for sensor %02x:%02x: "
              "{ value: %lu, threshold: %lu }",
              sensor_readings[i].address.u8[0],
              sensor_readings[i].address.u8[1], sensor_readings[i].value,
              sensor_readings[i].threshold);

          /* Value changed */
          restart_check = true;
        }
      }
    }

    /* Stop if no value changed */
    if (!restart_check) break;
  }
}

static void actuation_commands(void) {
  size_t i;
  const struct etc_event_t *event = etc_get_current_event();
  struct sensor_reading_t *sensor_reading = NULL;

  for (i = 0; i < NUM_SENSORS; ++i) {
    sensor_reading = &sensor_readings[i];

    /* Ignore if no command */
    if (sensor_reading->command == COMMAND_TYPE_NONE) continue;

    LOG_INFO(
        "Actuation command %d for sensor %02x:%02x on event "
        "{ seqn: %u, source: %02x:%02x }",
        sensor_reading->command, sensor_reading->address.u8[0],
        sensor_reading->address.u8[1], event->seqn, event->source.u8[0],
        event->source.u8[1]);

    /* Send command message via ETC */
    if (!etc_command(&sensor_reading->address, sensor_reading->command,
                     sensor_reading->threshold)) {
      LOG_ERROR(
          "Error sending ETC command %d for sensor %02x:%02x on event "
          "{ seqn: %u, source: %02x:%02x }",
          sensor_reading->command, sensor_reading->address.u8[0],
          sensor_reading->address.u8[1], event->seqn, event->source.u8[0],
          event->source.u8[1]);
      break;
    }
  }

  /* Reset */
  num_sensor_readings = 0;
  for (i = 0; i < NUM_SENSORS; ++i) {
    sensor_reading = &sensor_readings[i];

    sensor_reading->command = COMMAND_TYPE_NONE;
    sensor_reading->reading_available = false;
  }
}
