#ifndef _CONNECTION_H_
#define _CONNECTION_H_

#include <net/linkaddr.h>
#include <stdbool.h>
#include <sys/types.h>

/**
 * @brief Connection object.
 */
struct connection_t {
  /**
   * @brief Parent node address.
   */
  linkaddr_t parent_node;
  /**
   * @brief Sequence number.
   */
  uint16_t seqn;
  /**
   * @brief Hop number.
   */
  uint16_t hopn;
  /**
   * @brief RSSI parent node.
   */
  uint16_t rssi;
};

/**
 * @brief Best connection pointer.
 * Represents the best connection for communication
 * in the architecture.
 */
extern const struct connection_t *const best_conn;

/* --- BROADCAST --- */
/**
 * @brief Broadcast message types.
 */
enum broadcast_msg_type_t {
  /**
   * @brief Beacon message.
   */
  BROADCAST_MSG_TYPE_BEACON,
  /**
   * @brief Event message.
   */
  BROADCAST_MSG_TYPE_EVENT
};

/**
 * @brief Broadcast header.
 */
struct broadcast_hdr_t {
  /**
   * @brief Type of message.
   */
  enum broadcast_msg_type_t type;
} __attribute__((packed));

/* --- UNICAST --- */
/**
 * @brief Unicast message types.
 */
enum unicast_msg_type_t {
  /**
   * @brief Collect message.
   */
  UNICAST_MSG_TYPE_COLLECT
};

/**
 * @brief Unicast header.
 */
struct unicast_hdr_t {
  /**
   * @brief Type of message.
   */
  enum unicast_msg_type_t type;
} __attribute__((packed));

/* --- CALLBACKS --- */
/**
 * @brief Connection callbacks.
 */
struct connection_callbacks_t {
  /**
   * @brief Broadcast callbacks.
   */
  struct bc_t {
    /**
     * @brief Broadcast receive callbacks.
     */
    struct bc_recv_t {
      /**
       * @brief Beacon message callback.
       */
      void (*beacon)(const struct broadcast_hdr_t *header,
                     const linkaddr_t *sender);
      /**
       * @brief Event message callback.
       */
      void (*event)(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender);
    } recv;
  } bc;

  /**
   * @brief Unicast callbacks.
   */
  struct uc_t {
    /**
     * @brief Unicast receive callbacks.
     */
    struct uc_recv_t {
      /**
       * @brief Collect message callback.
       */
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
 * @brief Check if there is a valid connection.
 *
 * @return true Connected.
 * @return false Disconnected.
 */
bool connection_is_connected();

/**
 * @brief Send a broadcast message.
 *
 * @param type Message type.
 * @return 1 if the message has been sent.
 * 0 if the message could not be sent due to a generic error.
 * -1 if disconnected.
 * -2 if unable to allocate sufficient header space.
 */
int connection_broadcast_send(enum broadcast_msg_type_t type);

/**
 * @brief Send a unicast message to receiver.
 *
 * @param type Message type.
 * @param receiver
 * @return 1 if the message has been sent.
 * 0 if the message could not be sent due to a generic error.
 * -1 if disconnected.
 * -2 if unable to allocate sufficient header space.
 */
int connection_unicast_send(enum unicast_msg_type_t type,
                            const linkaddr_t *receiver);

#endif
