#include "beacon.h"

#include <stdio.h>

#include "config/config.h"

/**
 * @brief Beacon message structure.
 */
static struct beacon_msg_t {
  /**
   * @brief Sequence number.
   */
  uint16_t seqn;
  /**
   * @brief Quality metric.
   */
  uint16_t metric;
} __attribute__((packed));

/**
 * @brief Send beacon using the current seqn and metric.
 *
 * @param conn Pointer to an ETC connection object.
 */
static void send_beacon_message(const struct etc_conn_t *conn);

/**
 * @brief Broadcast receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
static void broadcast_recv_cb(struct broadcast_conn *bc_conn,
                              const linkaddr_t *sender);

/**
 * @brief Beacon timer callback.
 *
 * @param ptr An opaque pointer that will be supplied as an argument to the
 * callback function.
 */
static void beacon_timer_cb(void *ptr);

/**
 * @brief Callback structure for broadcast.
 */
static struct broadcast_callbacks broadcast_cb = {.recv = broadcast_recv_cb,
                                                  .sent = NULL};

void beacon_init(struct etc_conn_t *conn, uint16_t channel) {
  /* Open the underlying Rime primitive */
  broadcast_open(&conn->bc, channel, &broadcast_cb);

  /* Tree construction */
  if (conn->node_role == NODE_ROLE_CONTROLLER) {
    conn->metric = 0;
    /* Schedule the first beacon message flood */
    ctimer_set(&conn->beacon_timer, CLOCK_SECOND, beacon_timer_cb, conn);
  }
}

static void send_beacon_message(const struct etc_conn_t *conn) {
  /* Prepare beacon message */
  const struct beacon_msg_t beacon_msg = {.seqn = conn->beacon_seqn,
                                          .metric = conn->metric};

  /* Send beacon message in broadcast */
  packetbuf_clear();
  packetbuf_copyfrom(&beacon_msg, sizeof(beacon_msg));
  printf("Sending beacon message: { seqn: %d, metric: %d }\n",
         conn->beacon_seqn, conn->metric);
  broadcast_send(&conn->bc);
}

static void broadcast_recv_cb(struct broadcast_conn *bc_conn,
                              const linkaddr_t *sender) {
  struct beacon_msg_t beacon_msg;
  struct etc_conn_t *conn;
  int16_t rssi;

  conn = (struct etc_conn_t *)(((uint8_t *)bc_conn) -
                               offsetof(struct etc_conn_t, bc));

  /* Check received beacon message validity */
  if (packetbuf_datalen() != sizeof(struct beacon_msg_t)) {
    printf("Received beacon message has wrong size: %d\n", packetbuf_datalen());
    return;
  }
  memcpy(&beacon_msg, packetbuf_dataptr(), sizeof(struct beacon_msg_t));

  /* Read RSSI of last reception */
  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf(
      "Beacon message from %02x:%02x with rssi %d: "
      "{ seqn: %u, metric: %u}\n",
      sender->u8[0], sender->u8[1], rssi, beacon_msg.seqn, beacon_msg.metric);

  /* Analyze received beacon message based on RSSI, seqn, and metric */
  if (rssi < ETC_RSSI_THRESHOLD || beacon_msg.seqn < conn->beacon_seqn)
    return; /* Too weak or too old */
  if (beacon_msg.seqn == conn->beacon_seqn) {
    /* Beacon message not new, check metric */
    if (beacon_msg.metric + 1 >= conn->metric)
      return; /* Worse or equal than stored */
  }

  /* Memorize new parent, metric and seqn */
  linkaddr_copy(&conn->parent, sender);
  conn->metric = beacon_msg.metric + 1;
  conn->beacon_seqn = beacon_msg.seqn;
  printf("New parent %02x:%02x with metric %d\n", conn->parent.u8[0],
         conn->parent.u8[1], conn->metric);

  /* Schedule beacon message propagation */
  ctimer_set(&conn->beacon_timer, ETC_BEACON_FORWARD_DELAY, beacon_timer_cb,
             conn);
}

static void beacon_timer_cb(void *ptr) {
  struct etc_conn_t *conn = (struct etc_conn_t *)ptr;
  send_beacon_message(conn);

  if (conn->node_role == NODE_ROLE_CONTROLLER) {
    /* Rebuild tree from scratch after beacon interval */

    /* Increase beacon sequence number */
    conn->beacon_seqn += 1;
    /* Schedule next beacon message flood */
    ctimer_set(&conn->beacon_timer, ETC_BEACON_INTERVAL, beacon_timer_cb, conn);
  }
}
