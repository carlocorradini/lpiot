#include "beacon.h"

#include <stdio.h>

#include "config/config.h"

/**
 * @brief Beacon connection(s) object.
 * Ordered from best (index 0) to worst (index length-1).
 */
static struct connection_t connections[CONNECTION_BEACON_MAX_CONNECTIONS];

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
 * @param beacon_msg Beacon message to send.
 */
static void send_beacon_message(const struct beacon_msg_t *beacon_msg);

/**
 * @brief Beacon timer callback.
 *
 * @param ignored.
 */
static void beacon_timer_cb(void *ignored);

/**
 * @brief Reset connections to default values.
 */
static void reset_connections(void);

/**
 * @brief Reset connection at index in connections.
 *
 * @param index Connection index to reset.
 */
static void reset_connections_idx(size_t index);

/**
 * @brief Shift connections to right starting from index.
 * From parameter must be within the range of the array (0, length - 1).
 * Example:
 * [1 2 3 FROM -> 5 6 7] -> [1 2 3 ? 4 5 6]
 *
 * @param from Start index to shift from.
 */
static void shift_right_connections(size_t from);

/**
 * @brief Shift connections to left starting from index.
 * From parameter must be within the range of the array (0, length - 1).
 * Example:
 * [1 2 3 FROM <- 5 6 7] -> [1 2 3 5 6 7 ?]
 *
 * @param from Start index to shift from.
 */
static void shift_left_connections(size_t from);

void beacon_init(const struct connection_t *best_connection, node_role_t role) {
  node_role = role;

  /* Best connection is always first in connections */
  best_connection = &connections[0];

  /* Initialize connection structure */
  reset_connections();

  /* Tree construction */
  if (node_role == NODE_ROLE_CONTROLLER) {
    connections[0].hopn = 0;
    /* Schedule the first beacon message flood */
    ctimer_set(&beacon_timer, CLOCK_SECOND, beacon_timer_cb, NULL);
  }
}

static void send_beacon_message(const struct beacon_msg_t *beacon_msg) {
  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(beacon_msg, sizeof(struct beacon_msg_t));

  /* Send beacon message in broadcast */
  const int ret = connection_broadcast_send(BROADCAST_MSG_TYPE_BEACON);
  if (!ret) {
    /* Error */
    printf("Error sending beacon message: %d\n", ret);
    return;
  }
  printf("Sending beacon message: { seqn: %u, hopn: %u }\n", beacon_msg->seqn,
         beacon_msg->hopn);
}

void beacon_recv_cb(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender) {
  struct beacon_msg_t beacon_msg;
  uint16_t rssi;
  size_t connection_index = 0;

  /* Check received beacon message validity */
  if (packetbuf_datalen() != sizeof(struct beacon_msg_t)) {
    printf("Received beacon message wrong size: %u\n", packetbuf_datalen());
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
  if (rssi < ETC_RSSI_THRESHOLD) return;             /* Too Weak */
  if (beacon_msg.seqn < connections[0].seqn) return; /* Old */
  if (beacon_msg.seqn == connections[0].seqn) {
    /* Same sequence number, check... */
    for (connection_index = 0;
         connection_index < CONNECTION_BEACON_MAX_CONNECTIONS;
         ++connection_index) {
      if (beacon_msg.hopn + 1 > connections[connection_index].hopn)
        continue; /* Far */
      if (beacon_msg.hopn + 1 == connections[connection_index].hopn &&
          rssi < connections[connection_index].rssi)
        continue; /* Weak */
      /* Found better parent node than ith */
      break;
    }

    if (connection_index >= CONNECTION_BEACON_MAX_CONNECTIONS)
      return; /* Far or Weak */

    /* Right shift connections to accomodate better ith parent node */
    shift_right_connections(connection_index);
  } else {
    /* Greater sequence number, reset connections */
    reset_connections();
  }

  /* Save better ith parent node */
  linkaddr_copy(&connections[connection_index].parent_node, sender);
  connections[connection_index].seqn = beacon_msg.seqn;
  connections[connection_index].hopn = beacon_msg.hopn + 1;
  connections[connection_index].rssi = rssi;

  if (connection_index == 0) {
    /* New best (connection) parent */
    printf("New parent %02x:%02x: { hopn: %u, rssi: %d }\n",
           connections[0].parent_node.u8[0], connections[0].parent_node.u8[1],
           connections[0].hopn, connections[0].rssi);
    /* Schedule beacon message propagation only if best */
    ctimer_set(&beacon_timer, ETC_BEACON_FORWARD_DELAY, beacon_timer_cb, NULL);
  }
}

static void beacon_timer_cb(void *ignored) {
  /* Prepare beacon message */
  const struct beacon_msg_t beacon_msg = {.seqn = connections[0].seqn,
                                          .hopn = connections[0].hopn};

  /* Send beacon message */
  send_beacon_message(&beacon_msg);

  if (node_role == NODE_ROLE_CONTROLLER) {
    /* Rebuild tree from scratch */

    /* Increase beacon sequence number */
    connections[0].seqn += 1;
    /* Schedule next beacon message flood */
    ctimer_set(&beacon_timer, ETC_BEACON_INTERVAL, beacon_timer_cb, NULL);
  }
}

static void reset_connections(void) {
  size_t i;
  for (i = 0; i < CONNECTION_BEACON_MAX_CONNECTIONS; ++i) {
    reset_connections_idx(i);
  }
}

static void reset_connections_idx(size_t index) {
  if (index < 0 || index >= CONNECTION_BEACON_MAX_CONNECTIONS) return;

  linkaddr_copy(&connections[index].parent_node, &linkaddr_null);
  connections[index].seqn = 0;
  connections[index].hopn = UINT16_MAX;
  connections[index].rssi = ETC_RSSI_THRESHOLD;
}

static void shift_right_connections(size_t from) {
  if (from < 0 || from >= CONNECTION_BEACON_MAX_CONNECTIONS) return;

  size_t i;
  for (i = CONNECTION_BEACON_MAX_CONNECTIONS - 1; i > from; --i) {
    connections[i] = connections[i - 1];
  }

  reset_connections_idx(from);
}

static void shift_left_connections(size_t from) {
  if (from < 0 || from >= CONNECTION_BEACON_MAX_CONNECTIONS) return;

  size_t i;
  for (i = from; i < CONNECTION_BEACON_MAX_CONNECTIONS - 1; ++i) {
    connections[i] = connections[i + 1];
  }

  reset_connections_idx(CONNECTION_BEACON_MAX_CONNECTIONS - 1);
}
