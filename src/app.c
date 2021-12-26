#include <contiki.h>
#include <core/net/linkaddr.h>
#include <dev/button-sensor.h>
#include <leds.h>
#include <lib/random.h>
#include <net/netstack.h>
#include <net/rime/rime.h>
#include <stdbool.h>
#include <stdio.h>

#include "etc/etc.h"
#include "tools/simple-energest.h"

#define ETC_FIRST_CHANNEL (0xAA)
#define CONTROLLER_COLLECT_WAIT (CLOCK_SECOND * 10)

/* Sensor */
#define SENSOR_UPDATE_INTERVAL (CLOCK_SECOND * 7)
#define SENSOR_UPDATE_INCREMENT (random_rand() % 300)
#define SENSOR_STARTING_VALUE_STEP (1000)

/* Controller */
#define CONTROLLER_MAX_DIFF (10000)
#define CONTROLLER_MAX_THRESHOLD (50000)
#define CONTROLLER_CRITICAL_DIFF (15000)

#ifndef CONTIKI_TARGET_SKY
linkaddr_t etc_controller = {{0xF7, 0x9C}}; /* Firefly node 1 */
#define NUM_SENSORS 5
linkaddr_t etc_sensors[NUM_SENSORS] = {
    {{0xF3, 0x84}}, /* Firefly node 3 */
    {{0xF2, 0x33}}, /* Firefly node 12 */
    {{0xf3, 0x8b}}, /* Firefly node 18 */
    {{0xF3, 0x88}}, /* Firefly node 22*/
    {{0xF7, 0xE1}}  /* Firefly node 30 */
};
#else
linkaddr_t etc_controller = {{0x01, 0x00}}; /* Sky node 1 */
#define NUM_SENSORS 5
linkaddr_t etc_sensors[NUM_SENSORS] = {{{0x02, 0x00}},
                                       {{0x03, 0x00}},
                                       {{0x04, 0x00}},
                                       {{0x05, 0x00}},
                                       {{0x06, 0x00}}};
#endif

PROCESS(app_process, "App process");
AUTOSTART_PROCESSES(&app_process);

/* Utils */
node_role_t role() {
  // Controller
  if (linkaddr_cmp(&etc_controller, &linkaddr_node_addr)) {
    return NODE_ROLE_CONTROLLER;
  }
  // Sensor/Actuator
  for (uint i = 0; i < NUM_SENSORS; i++) {
    if (linkaddr_cmp(&etc_sensors[i], &linkaddr_node_addr)) {
      return NODE_ROLE_SENSOR_ACTUATOR;
    }
  }
  // Forwarder
  return NODE_ROLE_FORWARDER;
}

/* ETC connection */
static struct etc_conn_t etc_conn;
static void recv_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                    const linkaddr_t *source, uint32_t value,
                    uint32_t threshold);
static void ev_cb(const linkaddr_t *event_source, uint16_t event_seqn);
static void com_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                   command_type_t command, uint32_t threshold);
struct etc_callbacks_t cb = {.recv_cb = NULL, .ev_cb = NULL, .com_cb = NULL};

/* Sensor */
static bool is_sensor;
static uint32_t sensor_value;
static uint32_t sensor_threshold;
static struct ctimer sensor_timer;
static void sensor_timer_cb(void *ptr);

/* Controller */
/* Array for sensor readings */
typedef struct {
  linkaddr_t addr;
  uint16_t seqn;
  bool reading_available;
  uint32_t value;
  uint32_t threshold;
  command_type_t command;
} sensor_reading_t;
static sensor_reading_t sensor_readings[NUM_SENSORS];
static uint8_t num_sensor_readings;

/* Actuation functions */
static void actuation_logic();
static void actuation_commands();

/* Application */
PROCESS_THREAD(app_process, ev, data) {
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);

  // Start energest to estimate node duty cycle
  simple_energest_start();

  printf("App: I am node %02x:%02x\n", linkaddr_node_addr.u8[0],
         linkaddr_node_addr.u8[1]);

  while (true) {
    switch (role()) {
      case NODE_ROLE_CONTROLLER: {
        // Callbacks
        cb.ev_cb = ev_cb;
        cb.recv_cb = recv_cb;
        cb.com_cb = NULL;

        // Sensor structure
        for (uint i = 0; i < NUM_SENSORS; ++i) {
          linkaddr_copy(&sensor_readings[i].addr, &etc_sensors[i]);
          sensor_readings[i].reading_available = false;
          sensor_readings[i].threshold = CONTROLLER_MAX_DIFF;
        }
        num_sensor_readings = 0;

        // Open connection (builds the tree when started)
        etc_open(&etc_conn, ETC_FIRST_CHANNEL, NODE_ROLE_CONTROLLER, &cb,
                 etc_sensors, NUM_SENSORS);
        printf("App: Controller started\n");

        break;
      }
      case NODE_ROLE_SENSOR_ACTUATOR: {
        is_sensor = true;

        // Initialize sensed data and threshold
        uint sensor_index;
        for (uint i = 0; i < NUM_SENSORS; i++) {
          if (linkaddr_cmp(&etc_sensors[i], &linkaddr_node_addr)) {
            sensor_index = i;
            break;
          }
        }
        sensor_value = SENSOR_STARTING_VALUE_STEP * sensor_index;
        sensor_threshold = CONTROLLER_MAX_DIFF;

        // Set periodic update of the sensed value
        ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb,
                   NULL);

        // Set callbacks
        cb.ev_cb = NULL;
        cb.recv_cb = NULL;
        cb.com_cb = com_cb;

        // Open connection
        etc_open(&etc_conn, ETC_FIRST_CHANNEL, NODE_ROLE_SENSOR_ACTUATOR, &cb,
                 etc_sensors, NUM_SENSORS);
        printf("App: Sensor/actuator started\n");

        break;
      }
      case NODE_ROLE_FORWARDER: {
        // Open connection (no callback is set for forwarders)
        etc_open(&etc_conn, ETC_FIRST_CHANNEL, NODE_ROLE_FORWARDER, &cb,
                 etc_sensors, NUM_SENSORS);
        printf("App: Forwarder started\n");

        break;
      }
    }

    // Wait button press | node failure simulation
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    printf("App: Simulating node failure\n");
    etc_close(&etc_conn);
    NETSTACK_MAC.off(false);
    leds_on(LEDS_RED);

    // Wait button press | node failure recovery
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    printf("App: Simulating node recovery\n");
    NETSTACK_MAC.on();
    leds_off(LEDS_RED);
  }

  PROCESS_END();
}

/* Periodic function to update the sensed value (and trigger events) */
static void sensor_timer_cb(void *ptr) {
  sensor_value += SENSOR_UPDATE_INCREMENT;
  etc_update(sensor_value, sensor_threshold);
  printf("Reading (%lu, %lu)\n", sensor_value, sensor_threshold);
  if (sensor_value > sensor_threshold) {
    int ret = etc_trigger(&etc_conn, sensor_value, sensor_threshold);

    /* Logging (should not log if etc_trigger returns 0,
     * indicating that new events are currently being suppressed) */
    if (ret) {
      printf("TRIGGER [%02x:%02x, %u]\n", etc_conn.event_source.u8[0],
             etc_conn.event_source.u8[1], etc_conn.event_seqn);
    }
  }
  ctimer_set(&sensor_timer, SENSOR_UPDATE_INTERVAL, sensor_timer_cb, NULL);
}

/* Data collection reception callback.
 * The controller sets this callback to store the readings of all sensors.
 * When all readings have been collected, the controller can send commands.
 * You may send commands earlier if some data is missing after a timeout,
 * running actuation logic on the acquired data. */
static void recv_cb(const linkaddr_t *event_source, uint16_t event_seqn,
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
         etc_conn.event_source.u8[0], etc_conn.event_source.u8[1],
         etc_conn.event_seqn, source->u8[0], source->u8[1], value, threshold);

  /* If all data was collected, call actuation logic */
}

/* Event detection callback;
 * This callback notifies the controller of an ongoing event dissemination.
 * After this notification, the controller waits for sensor readings.
 * The event callback should come with the event_source (the address of the
 * sensor that generated the event) and the event_seqn (a growing sequence
 * number). The logging, reporting source and sequence number, can be
 * matched with data collection logging to count how many packets,
 * associated to this event, were received. */
static void ev_cb(const linkaddr_t *event_source, uint16_t event_seqn) {
  /* Check if the event is old and discard it in that case;
   * otherwise, update the current event being handled */

  /* Logging */
  printf("EVENT [%02x:%02x, %u]\n", etc_conn.event_source.u8[0],
         etc_conn.event_source.u8[1], etc_conn.event_seqn);

  /* Wait for sensor readings */
}

/* Command reception callback;
 * This callback notifies the sensor/actuator of a command from the
 controller.
 * In this system, commands can only be of 2 types:
 * - COMMAND_TYPE_RESET:      sensed value should go to 0, and the threshold
                              back to normal;
 * - COMMAND_TYPE_THRESHOLD:  sensed value should not be modified, but the
                              threshold should be increased */
static void com_cb(const linkaddr_t *event_source, uint16_t event_seqn,
                   command_type_t command, uint32_t threshold) {
  /* Logging (based on the source and sequence number in the command message
   * sent by the sink, to guarantee that command transmission and
   * actuation can be matched by the analysis scripts) */
  printf("ACTUATION [%02x:%02x, %u] %02x:%02x\n", event_source->u8[0],
         event_source->u8[1], event_seqn, linkaddr_node_addr.u8[0],
         linkaddr_node_addr.u8[1]);

  /* Execute commands */
}

/* The actuation logic to be called after sensor readings have been
 * collected. This functions checks for the steady state conditions and
 * assigns commands to all sensor/actuators that are violating them. Should
 * be called before actuation_commands(), which sends ACTUATION messages
 * based on the results of actuation_logic() */
void actuation_logic() {
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

/* Sends actuations for all sensors with a pending command.
 * actuation_commands() should be called after actuation_logic(), as
 * the logic sets the command for each sensor in their associated structure.
 */
void actuation_commands() {
  int i;
  for (i = 0; i < NUM_SENSORS; i++) {
    if (sensor_readings[i].command != COMMAND_TYPE_NONE) {
      etc_command(&etc_conn, &sensor_readings[i].addr,
                  sensor_readings[i].command, sensor_readings[i].threshold);

      /* Logging (based on the current event, expressed by source seqn) */
      printf("COMMAND [%02x:%02x, %u] %02x:%02x\n", etc_conn.event_source.u8[0],
             etc_conn.event_source.u8[1], etc_conn.event_seqn,
             sensor_readings[i].addr.u8[0], sensor_readings[i].addr.u8[1]);
    }
  }
}
