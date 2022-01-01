#ifndef _ETC_H_
#define _ETC_H_

#include <net/linkaddr.h>
#include <sys/ctimer.h>

#include "connection/connection.h"
#include "node/node.h"

/**
 * @brief Callback structure.
 */
struct etc_callbacks_t {
  /**
   * Data collection reception callback.
   * The Controller sets this callback to store the readings of all sensors.
   * When all readings have been collected, the Controller can send commands.
   *
   * @param event_source Address of the sensor that generated the event.
   * @param event_seqn Event sequence number.
   * @param sender Address of the sender node.
   * @param value Sensor value.
   * @param threshold Sensor threshold.
   */
  void (*receive_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     const linkaddr_t *sender, uint32_t value,
                     uint32_t threshold);

  /**
   * Event detection callback.
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
   * Command reception callback.
   * Notifies the Sensor/Actuator of a command from the Controller.
   *
   * @param event_source Event source node.
   * @param event_seqn Event sequence number.
   * @param command Command type.
   * @param threshold New threshold.
   */
  void (*command_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     enum command_type_t command, uint32_t threshold);
};

/**
 * @brief Event object.
 */
struct etc_event_t {
  /* Sequence number. */
  uint16_t seqn;
  /* Address of the generator node. */
  linkaddr_t source;
};

/* --- --- */
/**
 * @brief Open an ETC connection.
 *
 * @param channel Channel(s) on which the connection will operate.
 * @param callbacks Pointer to the callback structure.
 */
void etc_open(uint16_t channel, const struct etc_callbacks_t *callbacks);

/**
 * @brief Close an ETC connection.
 */
void etc_close(void);

/**
 * @brief Return the current event.
 *
 * @return Evenet data.
 */
const struct etc_event_t *etc_get_current_event(void);

/**
 * @brief Start event dissemination.
 * If events are suppressed no dissemination to avoid contention.
 * Used only by Sensor node.
 * Returns 0 if new events are currently suppressed.
 *
 * @param value Sensed value.
 * @param threshold Current threshold.
 * @return
 */
/* FIXME RETURN */
int etc_trigger(uint32_t value, uint32_t threshold);

/**
 * @brief Send the command to the receiver node.
 * Used only by Controller node.
 *
 * @param receiver Receiver node address.
 * @param command Command to send.
 * @param threshold New threshold.
 * @return
 */
/* FIXME RETURN */
int etc_command(const linkaddr_t *receiver, enum command_type_t command,
                uint32_t threshold);

#endif
