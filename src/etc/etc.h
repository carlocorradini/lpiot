#ifndef _ETC_H_
#define _ETC_H_

#include <contiki.h>
#include <core/net/linkaddr.h>
#include <net/netstack.h>
#include <net/rime/rime.h>
#include <stdbool.h>

#define BEACON_INTERVAL (CLOCK_SECOND * 30)
#define BEACON_FORWARD_DELAY (random_rand() % CLOCK_SECOND)
#define EVENT_FORWARD_DELAY (random_rand() % (CLOCK_SECOND / 10))
#define COLLECT_START_DELAY \
  (CLOCK_SECOND * 3 + random_rand() % (CLOCK_SECOND * 2))
/* Timeout for new event generation */
#define SUPPRESSION_TIMEOUT_NEW (CLOCK_SECOND * 12)
/* Timeout for event repropagation */
#define SUPPRESSION_TIMEOUT_PROP (SUPPRESSION_TIMEOUT_NEW - CLOCK_SECOND / 2)
/* Timeout after a command to disable suppression */
#define SUPPRESSION_TIMEOUT_END (CLOCK_SECOND / 2)
#define MAX_SENSORS (10)
#define RSSI_THRESHOLD (-95)

typedef enum {
  COMMAND_TYPE_NONE,
  COMMAND_TYPE_RESET,
  COMMAND_TYPE_THRESHOLD
} command_type_t;

typedef enum {
  NODE_ROLE_CONTROLLER,
  NODE_ROLE_SENSOR_ACTUATOR,
  NODE_ROLE_FORWARDER
} node_role_t;

/* Callback structure */
struct etc_callbacks_t {
  /* Controller callbacks */
  void (*receive_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     const linkaddr_t *source, uint32_t value,
                     uint32_t threshold);

  void (*event_cb)(const linkaddr_t *event_source, uint16_t event_seqn);

  /* Sensor/actuator callbacks */
  void (*command_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     command_type_t command, uint32_t threshold);
};

/* Connection object */
struct etc_conn_t {
  /* Connections */
  struct broadcast_conn bc;
  struct unicast_conn uc;
  linkaddr_t parent;
  uint16_t metric;
  uint16_t beacon_seqn;

  /* Application callbacks */
  const struct etc_callbacks_t *callbacks;

  /* Timers */
  struct ctimer beacon_timer;
  // Stop the generation of new events
  struct ctimer suppression_timer;

  // Stop (temporarily) the propagation of events from other nodes
  struct ctimer suppression_prop_timer;

  // Role (controller, forwarder, sensor/actuator)
  node_role_t node_role;

  // Current event handled by the node
  // useful to match logs of the control loop till actuation
  linkaddr_t event_source;
  uint16_t event_seqn;
};

/**
 * @brief Initialize a ETC connection.
 *
 * @param conn Pointer to a ETC connection object.
 * @param channels Starting channel (ETC may use multiple channels).
 * @param node_role Role of the node (controller, forwarders, sensor/actuator).
 * @param callbacks Pointer to the callback structure.
 * @param sensors Addresses of sensors.
 * @param num_sensors Number of sensors.
 *
 * @return true ETC connection succeeded.
 * @return false ETC connection failed.
 */
bool etc_open(struct etc_conn_t *conn, uint16_t channels, node_role_t node_role,
              const struct etc_callbacks_t *callbacks, linkaddr_t *sensors,
              uint8_t num_sensors);

/* Sensor functions */
void etc_update(uint32_t value, uint32_t threshold);

int etc_trigger(struct etc_conn_t *conn, uint32_t value, uint32_t threshold);

/* Controller function */
int etc_command(struct etc_conn_t *conn, const linkaddr_t *dest,
                command_type_t command, uint32_t threshold);

/* Close connection */
void etc_close(struct etc_conn_t *conn);

#endif
