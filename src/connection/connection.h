#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <net/linkaddr.h>
#include <stdbool.h>
#include <sys/types.h>

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
  BROADCAST_MSG_TYPE_EVENT
};

/**
 * @brief Broadcast header.
 */
struct broadcast_hdr_t {
  /* Type of message. */
  enum broadcast_msg_type_t type;
} __attribute__((packed));

/* --- UNICAST --- */
/**
 * @brief Unicast message types.
 */
enum unicast_msg_type_t {
  /* Collect message. */
  UNICAST_MSG_TYPE_COLLECT
};

/**
 * @brief Unicast header.
 */
struct unicast_hdr_t {
  /* Type of message. */
  enum unicast_msg_type_t type;
} __attribute__((packed));

/* --- CALLBACKS --- */
/**
 * @brief Connection callbacks.
 */
struct connection_callbacks_t {
  /* Broadcast callbacks. */
  struct bc_t {
    /* Broadcast receive callbacks. */
    struct bc_recv_t {
      /* Beacon message callback. */
      void (*beacon)(const struct broadcast_hdr_t *header,
                     const linkaddr_t *sender);
      /* Event message callback. */
      void (*event)(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender);
    } recv;
  } bc;

  /* Unicast callbacks. */
  struct uc_t {
    /* Unicast receive callbacks. */
    struct uc_recv_t {
      /* Collect message callback. */
      void (*collect)(const struct unicast_hdr_t *header,
                      const linkaddr_t *sender);
    } recv;
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
 *
 * @param type Message type.
 * @param receiver Receiver address.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
bool connection_unicast_send(enum unicast_msg_type_t type,
                             const linkaddr_t *receiver);

#endif
