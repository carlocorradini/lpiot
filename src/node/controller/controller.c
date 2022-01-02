#include "controller.h"

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
 * @brief Event detection callback.
 * Notifies of an ongoing event dissemination.
 * After this notification, the controller waits for sensor readings.
 *
 * @param event_seqn Event sequence number.
 * @param event_source Address of the sensor that generated the event.
 */
static void event_cb(uint16_t event_seqn, const linkaddr_t *event_source);

/* FXIME */
/**
 * @brief Data collection reception callback.
 * Store the readings of all sensors.
 * When all readings have been collected, the controller can send commands.
 * TODO
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 *
 * You may send commands earlier if some data is missing after a timeout,
 * running actuation logic on the acquired data.
 *
 * @param event_seqn Event sequence number.
 * @param event_source Address of the sensor that generated the event.
 * @param source Address of the source sensor.
 * @param value Sensor value.
 * @param threshold Sensor threshold.
 */
static void collect_cb(uint16_t event_seqn, const linkaddr_t *event_source,
                       const linkaddr_t *source, uint32_t value,
                       uint32_t threshold);

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
    sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;
    sensor_readings[i].reading_available = false;
    sensor_readings[i].command = COMMAND_TYPE_NONE;
  }
  num_sensor_readings = 0;

  /* Open ETC connection */
  etc_open(CONNECTION_CHANNEL, &cb);
}

static void event_cb(uint16_t event_seqn, const linkaddr_t *event_source) {
  struct sensor_reading_t *sensor_reading;
  size_t i;

  /* Find sensor reading */
  for (i = 0; i < NUM_SENSORS; ++i) {
    sensor_reading = &sensor_readings[i];
    if (linkaddr_cmp(event_source, &sensor_reading->address)) break; /* Found */
  }

  /* Check if event source is known */
  if (i >= NUM_SENSORS) {
    LOG_WARN("Event has unknown event source: %02x:%02x", event_source->u8[0],
             event_source->u8[1]);
    return;
  }

  LOG_INFO(
      "Received event: "
      "{ seqn: %u, source: %02x:%02x}",
      event_seqn, event_source->u8[0], event_source->u8[1]);

  /* Check if event is old */
  if (event_seqn != 0 && event_seqn <= sensor_reading->seqn) {
    LOG_WARN("Discarding event because last reading of seqn %u >= %u received",
             sensor_reading->seqn, event_seqn);
    return;
  }

  /* TODO Wait for sensor readings with timer */
}

static void collect_cb(uint16_t event_seqn, const linkaddr_t *event_source,
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
  const struct etc_event_t *event = etc_get_current_event();
  LOG_INFO("COLLECT [%02x:%02x, %u] %02x:%02x (%lu, %lu)", event->source.u8[0],
           event->source.u8[1], event->seqn, source->u8[0], source->u8[1],
           value, threshold);

  /* If all data was collected, call actuation logic */
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
    LOG_INFO("Controller: No data collected");
    return;
  }

  /* Debug: missing sensors */
  int i, j;
  for (i = 0; i < NUM_SENSORS; i++) {
    if (!sensor_readings[i].reading_available) {
      LOG_INFO("Controller: Missing %02x:%02x data",
               sensor_readings[i].address.u8[0],
               sensor_readings[i].address.u8[1]);
    }
  }

  /* Search for nodes in need of actuation */
  bool restart_check = false;
  while (true) {
    restart_check = false;

    /* Find min */
    size_t value_min = 0;
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

          LOG_INFO("Controller: Reset %02x:%02x (%lu, %lu)",
                   sensor_readings[i].address.u8[0],
                   sensor_readings[i].address.u8[1], sensor_readings[i].value,
                   sensor_readings[i].threshold);

          /* A value was changed, restart values check */
          restart_check = true;
        } else if (sensor_readings[i].value > sensor_readings[i].threshold) {
          sensor_readings[i].command = COMMAND_TYPE_THRESHOLD;
          sensor_readings[i].threshold += value_min;

          LOG_INFO("Controller: Update threshold %02x:%02x (%lu, %lu)",
                   sensor_readings[i].address.u8[0],
                   sensor_readings[i].address.u8[1], sensor_readings[i].value,
                   sensor_readings[i].threshold);

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
      etc_command(&sensor_readings[i].address, sensor_readings[i].command,
                  sensor_readings[i].threshold);

      /* Logging (based on the current event, expressed by source seqn) */
      const struct etc_event_t *event = etc_get_current_event();
      LOG_INFO("COMMAND [%02x:%02x, %u] %02x:%02x", event->source.u8[0],
               event->source.u8[1], event->seqn,
               sensor_readings[i].address.u8[0],
               sensor_readings[i].address.u8[1]);
    }
  }
}
