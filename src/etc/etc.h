#ifndef _ETC_H_
#define _ETC_H_

#include <net/linkaddr.h>
#include <net/rime/rime.h>
#include <stdbool.h>

/**
 * @brief Command types.
 */
typedef enum {
  /**
   * @brief Don't do anything (ignore).
   */
  COMMAND_TYPE_NONE,
  /**
   * @brief Sensed value should go to 0, and the threshold back to normal.
   */
  COMMAND_TYPE_RESET,
  /**
   * @brief Sensed value should not be modified, but the threshold should be
   * increased.
   */
  COMMAND_TYPE_THRESHOLD
} command_type_t;

/**
 * @brief Node roles.
 */
typedef enum {
  /**
   * @brief Controller node.
   */
  NODE_ROLE_CONTROLLER,
  /**
   * @brief Sensor/Actuator node.
   */
  NODE_ROLE_SENSOR_ACTUATOR,
  /**
   * @brief Forwarder node.
   */
  NODE_ROLE_FORWARDER
} node_role_t;

/**
 * @brief Callback structure.
 */
struct etc_callbacks_t {
  /**
   * @brief Data collection reception callback.
   * The Controller sets this callback to store the readings of all sensors.
   * When all readings have been collected, the Controller can send commands.
   *
   * @param event_source Address of the sensor that generated the event.
   * @param event_seqn Event sequence number.
   * @param source Address of the source sensor.
   * @param value Sensor's value.
   * @param threshold Sensor's threshold.
   */
  void (*receive_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     const linkaddr_t *source, uint32_t value,
                     uint32_t threshold);

  /**
   * @brief Event detection callback.
   * This callback notifies the Controller of an ongoing event dissemination.
   * After this notification, the Controller waits for sensor readings.
   * The event callback should come with the event_source (the address of the
   * sensor that generated the event) and the event_seqn (a growing sequence
   * number).
   *
   * @param event_source Address of the sensor that generated the event.
   * @param event_seqn Event sequence number.
   */
  void (*event_cb)(const linkaddr_t *event_source, uint16_t event_seqn);

  /**
   * @brief Command reception callback.
   * Notifies the Sensor/Actuator of a command from the Controller.
   *
   * @param event_source Event source node.
   * @param event_seqn Event sequence number.
   * @param command Command type.
   * @param threshold New threshold.
   */
  void (*command_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     command_type_t command, uint32_t threshold);
};

/**
 * @brief Connection object.
 */
struct etc_conn_t {
  /* --- Connection(s) */
  struct broadcast_conn bc;
  struct unicast_conn uc;
  linkaddr_t parent;
  uint16_t metric;
  uint16_t beacon_seqn;

  /* --- Callbacks */
  /**
   * @brief Application callbacks.
   */
  const struct etc_callbacks_t *callbacks;

  /* --- Timer(s) */
  /**
   * @brief Beacon message generation timer.
   */
  struct ctimer beacon_timer;

  /**
   * @brief Stop the generation of new events.
   */
  struct ctimer suppression_timer;

  /**
   * @brief Stop (temporarily) the propagation of events from other nodes.
   */
  struct ctimer suppression_prop_timer;

  /* --- */
  /**
   * @brief Node role.
   */
  node_role_t node_role;

  /* --- Event */
  /**
   * @brief Current handled event node address handled.
   */
  linkaddr_t event_source;

  /**
   * @brief Current handled event sequence number.
   */
  uint16_t event_seqn;
};

/**
 * @brief Initialize an ETC connection.
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
              const struct etc_callbacks_t *callbacks,
              const linkaddr_t *sensors, uint8_t num_sensors);

/**
 * @brief Terminate an ETC connection.
 *
 * @param conn Pointer to an ETC connection object.
 */
void etc_close(struct etc_conn_t *conn);

/* --- CONTROLLER --- */
/**
 * @brief Send command(s) to a given destination node.
 * Used only by the Controller.
 *
 * @param conn Pointer to an ETC connection object.
 * @param dest Destination node address.
 * @param command Command to send.
 * @param threshold New threshold.
 * @return int Command status
 */
int etc_command(struct etc_conn_t *conn, const linkaddr_t *dest,
                command_type_t command, uint32_t threshold);

/* --- SENSOR --- */
/**
 * @brief Share the most recent sensed value.
 * Used only by Sensor(s).
 *
 * @param value Sensed value.
 * @param threshold Current threshold.
 */
void etc_update(uint32_t value, uint32_t threshold);

/**
 * @brief Start event dissemination (unless events are suppressed to avoid
 * contention).
 * Used only by Sensor(s).
 * Returns 0 if new events are currently suppressed.
 *
 * @param conn Pointer to an ETC connection object.
 * @param value Sensed value.
 * @param threshold Current threshold.
 * @return int Trigger status
 */
int etc_trigger(struct etc_conn_t *conn, uint32_t value, uint32_t threshold);

#endif
