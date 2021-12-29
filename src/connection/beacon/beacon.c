#include "beacon.h"

#include <net/packetbuf.h>
#include <stdio.h>

#include "config/config.h"

/**
 * @brief Beacon connection object.
 */
static struct {
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
  packetbuf_attr_t rssi;
} connection;

/**
 * @brief Beacon message structure.
 */
struct beacon_msg_t {
  /**
   * @brief Sequence number.
   */
  uint16_t seqn;
  /**
   * @brief Hop number.
   */
  uint16_t hopn;
} __attribute__((packed));

/**
 * @brief Beacon message generation timer.
 */
static struct ctimer beacon_timer;

/**
 * @brief Node role.
 */
static node_role_t node_role;

/**
 * @brief Send beacon message.
 *
 * @param bc_conn Broadcast connection.
 * @param beacon_msg Beacon message to send.
 */
static void send_beacon_message(struct broadcast_conn *bc_conn,
                                const struct beacon_msg_t *beacon_msg);

/**
 * @brief Beacon timer callback.
 *
 * @param ptr Broadcast connection opaque pointer.
 */
static void beacon_timer_cb(void *ptr);

void beacon_init(struct broadcast_conn *bc_conn, node_role_t role) {
  node_role = role;

  /* Initialize connection structure */
  linkaddr_copy(&connection.parent_node, &linkaddr_null);
  connection.seqn = 0;
  connection.hopn = UINT16_MAX;
  connection.rssi = ETC_RSSI_THRESHOLD;

  /* Tree construction */
  if (node_role == NODE_ROLE_CONTROLLER) {
    connection.hopn = 0;
    /* Schedule the first beacon message flood */
    ctimer_set(&beacon_timer, CLOCK_SECOND, beacon_timer_cb, bc_conn);
  }
}

const struct connection_info_t beacon_connection_info(void) {
  return (const struct connection_info_t){.parent_node =
                                              &connection.parent_node};
}

static void send_beacon_message(struct broadcast_conn *bc_conn,
                                const struct beacon_msg_t *beacon_msg) {
  /* Send beacon message in broadcast */
  packetbuf_clear();
  packetbuf_copyfrom(beacon_msg, sizeof(struct beacon_msg_t));
  printf("Sending beacon message: { seqn: %d, hopn: %d }\n", beacon_msg->seqn,
         beacon_msg->hopn);
  broadcast_send(bc_conn);
}

void beacon_bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender) {
  struct beacon_msg_t beacon_msg;
  packetbuf_attr_t rssi;

  /* Check received beacon message validity */
  if (packetbuf_datalen() != sizeof(struct beacon_msg_t)) {
    printf("Received beacon message with wrong size: %d\n",
           packetbuf_datalen());
    return;
  }

  /* Copy beacon message */
  packetbuf_copyto(&beacon_msg);

  /* Read RSSI of last reception */
  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf(
      "Received beacon message from %02x:%02x with rssi %d: "
      "{ seqn: %u, hopn: %u}\n",
      sender->u8[0], sender->u8[1], rssi, beacon_msg.seqn, beacon_msg.hopn);

  /* Analyze received beacon message */
  if (beacon_msg.seqn < connection.seqn) return; /* Old */
  if (rssi < ETC_RSSI_THRESHOLD) return;         /* Too Weak */
  if (beacon_msg.seqn == connection.seqn) {
    /* Same sequence number, check... */
    if (beacon_msg.hopn + 1 > connection.hopn) return; /* Far */
    if (beacon_msg.hopn == connection.hopn && rssi < connection.rssi)
      return; /* Weak */
  }

  /* Better parent node */
  linkaddr_copy(&connection.parent_node, sender);
  connection.seqn = beacon_msg.seqn;
  connection.hopn = beacon_msg.hopn + 1;
  connection.rssi = rssi;
  printf("New parent %02x:%02x: { hopn: %d, rssi: %d }\n",
         connection.parent_node.u8[0], connection.parent_node.u8[1],
         connection.hopn, connection.rssi);

  /* Schedule beacon message propagation */
  ctimer_set(&beacon_timer, ETC_BEACON_FORWARD_DELAY, beacon_timer_cb, bc_conn);
}

static void beacon_timer_cb(void *ptr) {
  struct broadcast_conn *bc_conn = (struct broadcast_conn *)ptr;

  /* Prepare beacon message */
  const struct beacon_msg_t beacon_msg = {.seqn = connection.seqn,
                                          .hopn = connection.hopn};

  /* Send beacon message */
  send_beacon_message(bc_conn, &beacon_msg);

  if (node_role == NODE_ROLE_CONTROLLER) {
    /* Rebuild tree from scratch */

    /* Increase beacon sequence number */
    connection.seqn += 1;
    /* Schedule next beacon message flood */
    ctimer_set(&beacon_timer, ETC_BEACON_INTERVAL, beacon_timer_cb, bc_conn);
  }
}
