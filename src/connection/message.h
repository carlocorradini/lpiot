#ifndef _CONNECTION_MESSAGE_H_
#define _CONNECTION_MESSAGE_H_

#include <net/linkaddr.h>
#include <sys/types.h>

#include "node/node.h"

/* --- TYPE --- */
/**
 * @brief Broadcast message types.
 */
enum broadcast_msg_type_t {
  /* Beacon message. */
  BROADCAST_MSG_TYPE_BEACON,
  /* Event message. */
  BROADCAST_MSG_TYPE_EVENT,
  /**
   * Command message.
   * Only used in case of emergency.
   */
  BROADCAST_MSG_TYPE_EMERGENCY_COMMAND,
};

/**
 * @brief Unicast message types.
 */
enum unicast_msg_type_t {
  /* Collect message. */
  UNICAST_MSG_TYPE_COLLECT,
  /* Command message. */
  UNICAST_MSG_TYPE_COMMAND
};

/* --- MESSAGE --- */
/**
 * @brief Beacon message.
 */
struct beacon_msg_t {
  /* Sequence number. */
  uint16_t seqn;
  /* Hop number. */
  uint16_t hopn;
} __attribute__((packed));

/**
 * @brief Event message.
 */
struct event_msg_t {
  /* Event sequence number. */
  uint16_t seqn;
  /* Address of the sensor that generated the event. */
  linkaddr_t source;
} __attribute__((packed));

/**
 * @brief Collect message.
 */
struct collect_msg_t {
  /* Event sequence number. */
  uint16_t event_seqn;
  /* Address of the sensor that generated the event. */
  linkaddr_t event_source;
  /* Address of sender sensor node. */
  linkaddr_t sender;
  /* Node value. */
  uint32_t value;
  /* Node threshold. */
  uint32_t threshold;
} __attribute__((packed));

/**
 * @brief Command message.
 */
struct command_msg_t {
  /* Event sequence number. */
  uint16_t event_seqn;
  /* Address of the sensor that generated the event. */
  linkaddr_t event_source;
  /* Adress of receiver actuator node. */
  linkaddr_t receiver;
  /* Command type. */
  enum command_type_t command;
  /* New threshold. */
  uint32_t threshold;
} __attribute__((packed));

#endif
