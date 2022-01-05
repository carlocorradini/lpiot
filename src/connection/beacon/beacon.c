#include "beacon.h"

#include <net/packetbuf.h>

#include "config/config.h"
#include "logger/logger.h"
#include "node/node.h"

/**
 * @brief Beacon message structure.
 */
struct beacon_msg_t {
  /* Sequence number. */
  uint16_t seqn;
  /* Hop number. */
  uint16_t hopn;
} __attribute__((packed));

/**
 * @brief Connection(s) object.
 * Ordered from best (0) to worst (length-1).
 */
static struct connection_t connections[CONNECTION_BEACON_MAX_CONNECTIONS];

/**
 * @brief Beacon message generation timer.
 */
static struct ctimer beacon_timer;

/**
 * @brief Beacon timer callback.
 *
 * @param ignored.
 */
static void beacon_timer_cb(void *ignored);

/**
 * @brief Send beacon message.
 *
 * @param beacon_msg Beacon message to send.
 */
static void send_beacon_message(const struct beacon_msg_t *beacon_msg);

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
 * The from entry is resetted.
 * Example:
 * [1 2 3 FROM 5 6 7] -> [1 2 3 ? 4 5 6]
 * The ? entry is resetted.
 *
 * @param from Start index to shift from.
 */
static void shift_right_connections(size_t from);

/**
 * @brief Shift connections to left starting from index.
 * From parameter must be within the range of the array (0, length - 1).
 * The last entry is resetted.
 * Example:
 * [1 2 3 FROM 5 6 7] -> [1 2 3 5 6 7 ?]
 * The ? entry is resetted.
 *
 * @param from Start index to shift from.
 */
static void shift_left_connections(size_t from);

/**
 * @brief Print connection(s) array.
 */
static void print_connections(void);

/* --- --- */
void beacon_init(void) {
  /* Initialize connection structure */
  reset_connections();

  /* Tree construction */
  if (node_get_role() == NODE_ROLE_CONTROLLER) {
    connections[0].hopn = 0;
    /* Schedule the first beacon message flood */
    ctimer_set(&beacon_timer, CLOCK_SECOND, beacon_timer_cb, NULL);
  }
}

void beacon_terminate(void) {
  reset_connections();
  ctimer_stop(&beacon_timer);
}

const struct connection_t *beacon_get_conn(void) {
  /* Best connection is always the first */
  return &connections[0];
}

static void send_beacon_message(const struct beacon_msg_t *beacon_msg) {
  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(beacon_msg, sizeof(struct beacon_msg_t));

  /* Send beacon message in broadcast */
  const bool ret = connection_broadcast_send(BROADCAST_MSG_TYPE_BEACON);
  if (!ret) {
    /* Error */
    LOG_ERROR("Error sending beacon message");
    return;
  }
  LOG_DEBUG("Sending beacon message: { seqn: %u, hopn: %u }", beacon_msg->seqn,
            beacon_msg->hopn);
}

void beacon_recv_cb(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender) {
  struct beacon_msg_t beacon_msg;
  uint16_t rssi;
  size_t connection_index = 0;
  size_t i;

  /* Skip if controller node */
  if (node_get_role() == NODE_ROLE_CONTROLLER) return;

  /* Check received beacon message validity */
  if (packetbuf_datalen() != sizeof(beacon_msg)) {
    LOG_ERROR("Received beacon message wrong size: %u byte",
              packetbuf_datalen());
    return;
  }

  /* Copy beacon message */
  packetbuf_copyto(&beacon_msg);

  /* Read RSSI of last reception */
  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);

  LOG_DEBUG(
      "Received beacon message from %02x:%02x with rssi %d: "
      "{ seqn: %u, hopn: %u}",
      sender->u8[0], sender->u8[1], rssi, beacon_msg.seqn, beacon_msg.hopn);

  /* Analyze received beacon message */
  if (rssi < CONNECTION_RSSI_THRESHOLD) return; /* Too Weak */
  if (beacon_msg.seqn != 0 && beacon_msg.seqn < connections[0].seqn)
    return; /* Old (Keep in mind seqn overflow) */
  if (beacon_msg.seqn == connections[0].seqn) {
    /* Same sequence number, check... */
    for (connection_index = 0;
         connection_index < CONNECTION_BEACON_MAX_CONNECTIONS;
         ++connection_index) {
      if ((beacon_msg.seqn == 0 &&
           beacon_msg.seqn < connections[connection_index].seqn) ||
          beacon_msg.seqn > connections[connection_index].seqn) {
        break; /* Better -> New */
      }

      if (beacon_msg.hopn + 1 > connections[connection_index].hopn) {
        continue; /* Worse -> Far */
      }
      if (beacon_msg.hopn + 1 == connections[connection_index].hopn &&
          rssi <= connections[connection_index].rssi) {
        continue; /* Worse -> Weak */
      }

      /* Better -> Near or Strong */
      break;
    }

    if (connection_index >= CONNECTION_BEACON_MAX_CONNECTIONS)
      return; /* Far or Weak */
  }

  /* Remove (possible) duplicate parent node */
  i = 0;
  while (i < CONNECTION_BEACON_MAX_CONNECTIONS) {
    if (linkaddr_cmp(&connections[i].parent_node, sender)) {
      shift_left_connections(i);
      LOG_DEBUG("Removed duplicate parent node %02x:%02x", sender->u8[0],
                sender->u8[1]);
    } else {
      ++i;
    }
  }

  /* Right shift connections to accomodate better ith connection */
  shift_right_connections(connection_index);

  /* Save better ith parent node */
  linkaddr_copy(&connections[connection_index].parent_node, sender);
  connections[connection_index].seqn = beacon_msg.seqn;
  connections[connection_index].hopn = beacon_msg.hopn + 1;
  connections[connection_index].rssi = rssi;

  print_connections();

  if (connection_index == 0) {
    /* New best (connection) parent */
    LOG_INFO("New parent %02x:%02x: { hopn: %u, rssi: %d }",
             connections[0].parent_node.u8[0], connections[0].parent_node.u8[1],
             connections[0].hopn, connections[0].rssi);
    /* Schedule beacon message propagation only if best */
    ctimer_set(&beacon_timer, CONNECTION_BEACON_FORWARD_DELAY, beacon_timer_cb,
               NULL);
  } else {
    LOG_DEBUG("Backup parent %02x:%02x at %d: { hopn: %u, rssi: %d }",
              connections[connection_index].parent_node.u8[0],
              connections[connection_index].parent_node.u8[1], connection_index,
              connections[connection_index].hopn,
              connections[connection_index].rssi);
  }
}

static void beacon_timer_cb(void *ignored) {
  /* Prepare beacon message */
  const struct beacon_msg_t beacon_msg = {.seqn = connections[0].seqn,
                                          .hopn = connections[0].hopn};

  /* Send beacon message */
  send_beacon_message(&beacon_msg);

  if (node_get_role() == NODE_ROLE_CONTROLLER) {
    /* Rebuild tree from scratch */

    /* Increase beacon sequence number */
    connections[0].seqn += 1;
    /* Schedule next beacon message flood */
    ctimer_set(&beacon_timer, CONNECTION_BEACON_INTERVAL, beacon_timer_cb,
               NULL);
  }
}

/* --- CONNECTIONS --- */
void beacon_invalidate_connection(void) {
  /* Shift connections to left removing current best connection */
  shift_left_connections(0);
  print_connections();
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
  connections[index].rssi = CONNECTION_RSSI_THRESHOLD;
}

static void shift_right_connections(size_t from) {
  if (from < 0 || from >= CONNECTION_BEACON_MAX_CONNECTIONS) return;

  size_t i;
  for (i = CONNECTION_BEACON_MAX_CONNECTIONS - 1; i > from; --i) {
    linkaddr_copy(&connections[i].parent_node, &connections[i - 1].parent_node);
    connections[i].seqn = connections[i - 1].seqn;
    connections[i].hopn = connections[i - 1].hopn;
    connections[i].rssi = connections[i - 1].rssi;
  }

  reset_connections_idx(from);
}

static void shift_left_connections(size_t from) {
  if (from < 0 || from >= CONNECTION_BEACON_MAX_CONNECTIONS) return;

  size_t i;
  for (i = from; i < CONNECTION_BEACON_MAX_CONNECTIONS - 1; ++i) {
    linkaddr_copy(&connections[i].parent_node, &connections[i + 1].parent_node);
    connections[i].seqn = connections[i + 1].seqn;
    connections[i].hopn = connections[i + 1].hopn;
    connections[i].rssi = connections[i + 1].rssi;
  }

  reset_connections_idx(i);
}

static void print_connections(void) {
  if (!logger_is_enabled(LOG_LEVEL_DEBUG)) return;

  size_t i;
  const struct connection_t *conn;

  logger_set_newline(false);
  LOG_DEBUG("Connections: ");
  printf("[ ");
  for (i = 0; i < CONNECTION_BEACON_MAX_CONNECTIONS; ++i) {
    conn = &connections[i];
    printf("%u{ parent_node: %02x:%02x, seqn: %u, hopn: %u, rssi: %d } ", i,
           conn->parent_node.u8[0], conn->parent_node.u8[1], conn->seqn,
           conn->hopn, conn->rssi);
  }
  printf("]\n");
  logger_set_newline(true);
}
