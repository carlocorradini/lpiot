#include "etc.h"

#include <stdio.h>

#include "config/config.h"

/* A simple debug system to enable/disable some printfs */
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* --- Topology information (parents, metrics...) */
/* ... */

/* --- Forwarders (routes to sensors/actuators) */
/* ... */

/* --- Declarations for the callbacks of dedicated connection objects */
/**
 * @brief Beacon receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
static void broadcast_recv(struct broadcast_conn *bc_conn,
                           const linkaddr_t *sender);

/**
 * @brief Data receive callback.
 *
 * @param uc_conn Unicast connection.
 * @param sender Address of the sender node.
 */
static void unicast_recv(struct unicast_conn *uc_conn,
                         const linkaddr_t *sender);

/**
 * @brief Beacon timer callback.
 *
 * @param ptr An opaque pointer that will be supplied as an argument to the
 * callback function.
 */
static void beacon_timer_cb(void *ptr);

/* --- Rime Callback structures */
/**
 * @brief Callback structure for broadcast.
 */
static struct broadcast_callbacks broadcast_cb = {.recv = broadcast_recv,
                                                  .sent = NULL};

/**
 * @brief Callback structure for unicast.
 */
static struct unicast_callbacks unicast_cb = {.recv = unicast_recv,
                                              .sent = NULL};

/*---------------------------------------------------------------------------*/
/*                           Application Interface                           */
/*---------------------------------------------------------------------------*/
bool etc_open(struct etc_conn_t *conn, uint16_t channels, node_role_t node_role,
              const struct etc_callbacks_t *callbacks,
              const linkaddr_t *sensors, uint8_t num_sensors) {
  /* Initialize connector structure */
  linkaddr_copy(&conn->parent, &linkaddr_null);
  conn->metric = UINT16_MAX;
  conn->beacon_seqn = 0;
  conn->callbacks = callbacks;
  conn->node_role = node_role;

  /* Open the underlying Rime primitives */
  broadcast_open(&conn->bc, channels, &broadcast_cb);
  unicast_open(&conn->uc, channels + 1, &unicast_cb);

  /* Initialize sensors forwarding structure */

  /* Tree construction */
  if (conn->node_role == NODE_ROLE_CONTROLLER) {
    conn->metric = 0;
    /* Schedule the first beacon message flood */
    ctimer_set(&conn->beacon_timer, CLOCK_SECOND, beacon_timer_cb, conn);
  }
}

void etc_close(struct etc_conn_t *conn) {
  /* Turn off connections to ignore any incoming packet
   * and stop transmitting */
}

/* --- CONTROLLER--- */
int etc_command(struct etc_conn_t *conn, const linkaddr_t *dest,
                command_type_t command, uint32_t threshold) {
  /* Prepare and send command */
}

/* --- SENSOR --- */
void etc_update(uint32_t value, uint32_t threshold) {
  /* Update local value and threshold, to be sent in case of event */
}

int etc_trigger(struct etc_conn_t *conn, uint32_t value, uint32_t threshold) {
  /* Prepare event message */

  /* Suppress other events for a given time window */

  /* Send event */
}

/*---------------------------------------------------------------------------*/
/*                              Beacon Handling                              */
/*---------------------------------------------------------------------------*/
/**
 * @brief Beacon message structure.
 */
static struct beacon_msg_t {
  uint16_t seqn;
  uint16_t metric;
} __attribute__((packed));

/**
 * @brief Send beacon using the current seqn and metric.
 *
 * @param conn Pointer to an ETC connection object.
 */
static void send_beacon_message(const struct etc_conn_t *conn) {
  /* Prepare beacon message */
  const struct beacon_msg_t message = {.seqn = conn->beacon_seqn,
                                       .metric = conn->metric};

  /* Send beacon message in broadcast */
  packetbuf_clear();
  packetbuf_copyfrom(&message, sizeof(message));
  PRINTF("[ETC]: Sending beacon message: { seqn: %d, metric: %d }\n",
         conn->beacon_seqn, conn->metric);
  broadcast_send(&conn->bc);
}

/**
 * @brief Beacon timer callback.
 *
 * @param ptr An opaque pointer that will be supplied as an argument to the
 * callback function.
 */
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

static void broadcast_recv(struct broadcast_conn *bc_conn,
                           const linkaddr_t *sender) {
  struct beacon_msg_t message;
  struct etc_conn_t *conn;
  int16_t rssi;

  conn = (struct etc_conn_t *)(((uint8_t *)bc_conn) -
                               offsetof(struct etc_conn_t, bc));

  /* Check received broadcast message validity */
  if (packetbuf_datalen() != sizeof(struct beacon_msg_t)) {
    printf("[ETC]: Broadcast message of wrong size: %d\n", packetbuf_datalen());
    return;
  }
  memcpy(&message, packetbuf_dataptr(), sizeof(struct beacon_msg_t));

  /* Read RSSI of last reception */
  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  PRINTF(
      "[ETC]: broadcast_recv beacon message from %02x:%02x: { seqn: %u, "
      "metric: %u, "
      "rssi: %d }\n",
      sender->u8[0], sender->u8[1], beacon.seqn, beacon.metric, rssi);

  /* Analyze received beacon message based on RSSI, seqn, and metric */
  if (rssi < ETC_RSSI_THRESHOLD || message.seqn < conn->beacon_seqn)
    return; /* Too weak or too old */
  if (message.seqn == conn->beacon_seqn) {
    /* Beacon message not new, check metric */
    if (message.metric + 1 >= conn->metric)
      return; /* Worse or equal than stored */
  }

  /* Memorize new parent, metric and seqn */
  linkaddr_copy(&conn->parent, sender);
  conn->metric = message.metric + 1;
  conn->beacon_seqn = message.seqn;
  PRINTF("[ETC]: New parent %02x:%02x. My metric %d\n", conn->paren->u8[0],
         conn->paren->u8[1], conn->metric);

  /* Schedule beacon message propagation */
  ctimer_set(&conn->beacon_timer, ETC_BEACON_FORWARD_DELAY, beacon_timer_cb,
             conn);
}

/*---------------------------------------------------------------------------*/
/*                               Event Handling                              */
/*---------------------------------------------------------------------------*/
/**
 * @brief  Event message structure.
 * Combining event source (address of the sensor
 * generating the event) and the sequence number.
 */
static struct event_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
} __attribute__((packed));

/*---------------------------------------------------------------------------*/
/*                               Data Handling                               */
/*---------------------------------------------------------------------------*/
/**
 * @brief Header structure for data packets.
 */
static struct collect_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
  uint8_t hops;
} __attribute__((packed));

/**
 * @brief Send collect message towards the Controller node.
 * Returns:
 *  -1 Node is disconnected (no parent).
 *  -2 packetbuf_hdralloc(...) error.
 *  ?  unicast_send(...) return code.
 *
 * @param conn Pointer to an ETC connection object.
 * @return int Status code.
 */
static int send_collect_message(struct etc_conn_t *conn) {
  /* Prepare collect message */
  struct collect_msg_t message = {.event_source = linkaddr_node_addr,
                                  .event_seqn = 0,
                                  /*FIXME SEQUENCE hardcoded!*/ .hops = 0};

  /* Check if node is disconnected (no parent) */
  if (linkaddr_cmp(&conn->parent, &linkaddr_null)) return -1;

  /* Check if header could be extended */
  if (!packetbuf_hdralloc(sizeof(struct collect_msg_t))) return -2;

  /* Insert header in the packet buffer */
  memcpy(packetbuf_hdrptr(), &message, sizeof(message));

  /* TODO Finish better */
  PRINTF("[ETC]: Sending collect message to %02x:%02x: { }\n",
         onn->paren->u8[0], conn->paren->u8[1]);

  /* Send packet to parent node */
  return unicast_send(&conn->uc, &conn->parent);
}

static void unicast_recv(struct unicast_conn *uc_conn,
                         const linkaddr_t *sender) {
  struct etc_conn_t *conn;
  struct collect_msg_t header;

  conn = (struct etc_conn_t *)(((uint8_t *)uc_conn) -
                               offsetof(struct etc_conn_t, uc));

  /* Check received unicast message validity */
  if (packetbuf_datalen() < sizeof(struct collect_msg_t)) {
    printf("[ETC]: Unicast message of wrong size: %d\n", packetbuf_datalen());
    return;
  }

  /* Extract header */
  memcpy(&header, packetbuf_dataptr(), sizeof(header));
  /* Update hop count */
  header.hops = header.hops + 1;
  PRINTF("[ETC]: Data packet rcvd -- source: %02x:%02x, hops: %u\n",
         header.event_source.u8[0], header.event_source.u8[1], header.hops);

  switch (conn->node_role) {
    case NODE_ROLE_CONTROLLER: {
      /* Remove header to make packetbuf_dataptr() point to the beginning of
       * the application payload */
      if (packetbuf_hdrreduce(sizeof(struct collect_msg_t))) {
        /* FIXME Parametri dal payload */
        /* Call controller callback function */
        conn->callbacks->receive_cb(&header.event_source, header.event_seqn,
                                    NULL, 7, 7);
      } else {
        printf("[ETC]: ERROR, header could not be reduced!");
      }

      break;
    }
    case NODE_ROLE_SENSOR_ACTUATOR:
    case NODE_ROLE_FORWARDER: {
      /* Detect parent node disconnection */
      if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {
        printf(
            "[ETC]: ERROR, unable to forward data packet -- "
            "source: %02x:%02x, hops: %u\n",
            header.event_source.u8[0], header.event_source.u8[1], header.hops);
        return;
      }

      /* Update header in the packet buffer */
      memcpy(packetbuf_dataptr(), &header, sizeof(header));
      /* Send unicast message to parent */
      unicast_send(&conn->uc, &conn->parent);

      break;
    }
  }
}

/*---------------------------------------------------------------------------*/
/*                               Command Handling */
/*---------------------------------------------------------------------------*/
/**
 * @brief Header structure for command packets.
 */
static struct command_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
  /* ... */
} __attribute__((packed));
