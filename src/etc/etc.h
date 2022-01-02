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
   * Event detection callback.
   * Notifies the Controller of an ongoing event dissemination.
   * After this notification, the Controller waits for sensor readings.
   *
   * @param event_source Address of the sensor that generated the event.
   * @param event_seqn Event sequence number.
   */
  void (*event_cb)(const linkaddr_t *event_source, uint16_t event_seqn);

  /**
   * Data collection reception callback.
   * Notifies the Controller to store the reading of the Sensor.
   * When all readings have been collected, the Controller can send commands.
   *
   * @param event_source Address of the sensor that generated the event.
   * @param event_seqn Event sequence number.
   * @param sender Address of the sensor node.
   * @param value Sensor value.
   * @param threshold Sensor threshold.
   */
  void (*collect_cb)(const linkaddr_t *event_source, uint16_t event_seqn,
                     const linkaddr_t *sender, uint32_t value,
                     uint32_t threshold);

  /**
   * Command reception callback.
   * Notifies the Sensor/Actuator of a command from the Controller.
   *
   * @param event_source Address of the sensor that generated the event.
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
 *
 * @param value Sensed value.
 * @param threshold Current threshold.
 * @return true Started event dissemination.
 * @return false Event(s) are suppressed.
 */
bool etc_trigger(uint32_t value, uint32_t threshold);

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
