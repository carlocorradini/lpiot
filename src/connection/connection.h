#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <net/linkaddr.h>
#include <stdbool.h>
#include <sys/types.h>

#include "node/node.h"

/**
 * @brief Connection object.
 */
struct connection_t {
  /* Parent node address. */
  linkaddr_t parent_node;
  /* Sequence number. */
  uint16_t seqn;
  /* Hop number. */
  uint16_t hopn;
  /* RSSI parent node. */
  uint16_t rssi;
};

/* --- BROADCAST --- */
/**
 * @brief Broadcast message types.
 */
enum broadcast_msg_type_t {
  /* Beacon message. */
  BROADCAST_MSG_TYPE_BEACON,
  /* Event message. */
  BROADCAST_MSG_TYPE_EVENT,
  /* Forward discovery request. */
  BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_REQUEST,
  /* Forward discovery response. */
  BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_RESPONSE
};

/**
 * @brief Broadcast header.
 */
struct broadcast_hdr_t {
  /* Type of message. */
  enum broadcast_msg_type_t type;
} __attribute__((packed));

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
 * @brief Forward discovery message.
 */
struct forward_discovery_msg_t {
  /* Sensor address. */
  linkaddr_t sensor;
  /* Hop distance. */
  uint8_t distance;
};

/* --- UNICAST --- */
/**
 * @brief Unicast message types.
 */
enum unicast_msg_type_t {
  /* Collect message. */
  UNICAST_MSG_TYPE_COLLECT,
  /* Command message. */
  UNICAST_MSG_TYPE_COMMAND
};

/**
 * @brief Unicast header.
 */
struct unicast_hdr_t {
  /* Type of message. */
  enum unicast_msg_type_t type;
  /* Hop count. */
  uint8_t hops;
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

/* --- CALLBACKS --- */
/**
 * @brief Connection callbacks.
 */
struct connection_callbacks_t {
  /* Broadcast callbacks. */
  struct bc_t {
    /**
     * @brief Broadcast receive callback.
     *
     * @param header Broadcast header.
     * @param sender Address of the sender node.
     */
    void (*recv)(const struct broadcast_hdr_t *header,
                 const linkaddr_t *sender);

    /**
     * @brief Broadcast sent callback.
     *
     * @param status Status code.
     * @param num_tx Number of transmission(s).
     */
    void (*sent)(int status, int num_tx);
  } bc;

  /* Unicast callbacks. */
  struct uc_t {
    /**
     * @brief Unicast receive callback.
     *
     * @param header Unicast header.
     * @param sender Address of the sender node.
     */
    void (*recv)(const struct unicast_hdr_t *header, const linkaddr_t *sender);

    /**
     * @brief Unicast sent callback.
     * If status is true the message has been sent, otherwise something bad
     * happened.
     *
     * @param status Status.
     */
    void (*sent)(bool status);
  } uc;
};

/* --- --- */
/**
 * @brief Open connection(s).
 *
 * @param channel Channel(s) on which the connection will operate.
 * @param callbacks Connection callbacks pointer.
 */
void connection_open(uint16_t channel,
                     const struct connection_callbacks_t *callbacks);

/**
 * @brief Close connection(s).
 */
void connection_close(void);

/**
 * @brief Return connection status.
 * Simply return true if the connection is established.
 *
 * @return true Connected.
 * @return false Disconnected.
 */
bool connection_is_connected(void);

/**
 * @brief Return the established connection.
 * Note that the returned connection could not be valid.
 * Always check connection_is_connected() before using connection.
 *
 * @return Established connection.
 */
const struct connection_t *connection_get_conn(void);

/**
 * @brief Invalidate current connection.
 *
 * @return true Backup connection available.
 * @return false No backup connection available.
 */
bool connection_invalidate(void);

/**
 * @brief Send a broadcast message.
 * A header is added.
 *
 * @param type Message type.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
bool connection_broadcast_send(enum broadcast_msg_type_t type);

/**
 * @brief Send a unicast message to receiver.
 * A header is added.
 * If no routing final_receiver should be NULL.
 *
 * @param header Header.
 * @param receiver Receiver address.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
bool connection_unicast_send(const struct unicast_hdr_t *uc_header,
                             const linkaddr_t *receiver);

#endif
