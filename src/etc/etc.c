#include "etc.h"

#include <stdbool.h>
#include <stdio.h>

#include "contiki.h"
#include "core/net/linkaddr.h"
#include "leds.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/rime/rime.h"

/* A simple debug system to enable/disable some printfs */
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/* Topology information (parents, metrics...) */
/* ... */

/* Forwarders (routes to sensors/actuators) */
/* ... */

/* Declarations for the callbacks of dedicated connection objects */
void bc_recv(struct broadcast_conn *conn, const linkaddr_t *sender);
void uc_recv(struct unicast_conn *conn, const linkaddr_t *from);
void beacon_timer_cb(void *ptr);

/* Rime Callback structures */
struct broadcast_callbacks broadcast_cb = {.recv = bc_recv, .sent = NULL};
struct unicast_callbacks unicast_cb = {.recv = uc_recv, .sent = NULL};

/*---------------------------------------------------------------------------*/
/*                           Application Interface                           */
/*---------------------------------------------------------------------------*/
bool etc_open(struct etc_conn_t *conn, uint16_t channels, node_role_t node_role,
              const struct etc_callbacks_t *callbacks, linkaddr_t *sensors,
              uint8_t num_sensors) {
  // Initialize connector structure
  linkaddr_copy(&conn->parent, &linkaddr_null);
  conn->metric = UINT16_MAX;
  conn->beacon_seqn = 0;
  conn->callbacks = callbacks;

  // Open the underlying Rime primitives
  broadcast_open(&conn->bc, channels, &broadcast_cb);
  unicast_open(&conn->uc, channels + 1, &unicast_cb);

  // Initialize sensors forwarding structure

  // Tree construction
  if (node_role == NODE_ROLE_CONTROLLER) {
    conn->metric = 0;
    // Schedule the first beacon message flood
    ctimer_set(&conn->beacon_timer, CLOCK_SECOND, beacon_timer_cb, conn);
  }
}

/* Turn off the protocol */
void etc_close(struct etc_conn_t *conn) {
  /* Turn off connections to ignore any incoming packet
   * and stop transmitting */
}

/* Used by the app to share the most recent sensed value;
 * ONLY USED BY SENSORS */
void etc_update(uint32_t value, uint32_t threshold) {
  /* Update local value and threshold, to be sent in case of event */
}

/* Start event dissemination (unless events are being suppressed to avoid
 * contention).
 * Returns 0 if new events are currently being suppressed.
 * ONLY USED BY SENSORS */
int etc_trigger(struct etc_conn_t *conn, uint32_t value, uint32_t threshold) {
  /* Prepare event message */

  /* Suppress other events for a given time window */

  /* Send event */
}

/* Called by the controller to send commands to a given destination.
 * ONLY USED BY CONTROLLER */
int etc_command(struct etc_conn_t *conn, const linkaddr_t *dest,
                command_type_t command, uint32_t threshold) {
  /* Prepare and send command */
}

/*---------------------------------------------------------------------------*/
/*                              Beacon Handling                              */
/*---------------------------------------------------------------------------*/
/* Beacon message structure */
struct beacon_msg_t {
  uint16_t seqn;
  uint16_t metric;
} __attribute__((packed));

/* Send beacon using the current seqn and metric */
void send_beacon(const struct etc_conn_t *conn) {
  // Prepare beacon message
  const struct beacon_msg_t message = {.seqn = conn->beacon_seqn,
                                       .metric = conn->metric};

  // Send beacon message in broadcast
  packetbuf_clear();
  packetbuf_copyfrom(&message, sizeof(message));
  PRINTF("[ETC]: Sending beacon message: { seqn: %d, metric: %d }\n",
         conn->beacon_seqn, conn->metric);
  broadcast_send(&conn->bc);
}

/* Beacon timer callback */
void beacon_timer_cb(void *ptr) {
  struct etc_conn_t *conn = (struct etc_conn_t *)ptr;
  send_beacon(conn);

  if (conn->node_role == NODE_ROLE_CONTROLLER) {
    // Rebuild tree from scratch after beacon interval

    // Increase beacon sequence number
    conn->beacon_seqn += 1;
    // Schedule next beacon message flood
    ctimer_set(&conn->beacon_timer, BEACON_INTERVAL, beacon_timer_cb, conn);
  }
}

/* Beacon receive callback */
void bc_recv(struct broadcast_conn *bc_conn, const linkaddr_t *sender) {
  struct beacon_msg_t message;
  struct etc_conn_t *conn;
  int16_t rssi;

  conn = (struct etc_conn_t *)(((uint8_t *)bc_conn) -
                               offsetof(struct etc_conn_t, bc));

  // Check received broadcast packet validity
  if (packetbuf_datalen() != sizeof(struct beacon_msg_t)) {
    printf("[ETC]: Broadcast packet of wrong size: %d\n", packetbuf_datalen());
    return;
  }
  memcpy(&message, packetbuf_dataptr(), sizeof(struct beacon_msg_t));

  // Read RSSI of last reception
  rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  PRINTF(
      "[ETC]: Broadcast recv beacon from %02x:%02x: { seqn: %u, metric: %u, "
      "rssi: %d }\n",
      sender->u8[0], sender->u8[1], beacon.seqn, beacon.metric, rssi);

  // Analyze the received beacon message based on RSSI, seqn, and metric.
  if (rssi < RSSI_THRESHOLD || message.seqn < conn->beacon_seqn)
    return;  // Too weak or too old
  if (message.seqn == conn->beacon_seqn) {
    // Beacon message not new, check metric
    if (message.metric + 1 >= conn->metric)
      return;  // Worse or equal than stored
  }

  // Memorize new parent, metric and seqn
  linkaddr_copy(&conn->parent, sender);
  conn->metric = message.metric + 1;
  conn->beacon_seqn = message.seqn;
  PRINTF("[ETC]: New parent %02x:%02x. My metric %d\n", sender->u8[0],
         sender->u8[1], conn->metric);

  /* Schedule beacon message propagation */
  ctimer_set(&conn->beacon_timer, BEACON_FORWARD_DELAY, beacon_timer_cb, conn);
}

/*---------------------------------------------------------------------------*/
/*                               Event Handling                              */
/*---------------------------------------------------------------------------*/
/* Event message structure, combining event source (address of the sensor
 * generating the event) and a sequence number. */
struct event_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
} __attribute__((packed));

/*---------------------------------------------------------------------------*/
/*                               Data Handling                               */
/*---------------------------------------------------------------------------*/
/* Header structure for data packets */
struct collect_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
  uint8_t hops;
} __attribute__((packed));

/* Data receive callback */
void uc_recv(struct unicast_conn *uc_conn, const linkaddr_t *from) {
  struct etc_conn_t *conn;
  struct collect_msg_t header;

  conn = (struct etc_conn_t *)(((uint8_t *)uc_conn) -
                               offsetof(struct etc_conn_t, uc));

  // Check received unicast packet validity
  if (packetbuf_datalen() < sizeof(struct collect_msg_t)) {
    printf("[ETC]: Unicast packet of wrong size: %d\n", packetbuf_datalen());
    return;
  }

  // Extract header
  memcpy(&header, packetbuf_dataptr(), sizeof(header));
  // Update hop count
  header.hops = header.hops + 1;
  PRINTF("[ETC]: Data packet rcvd -- source: %02x:%02x, hops: %u\n",
         header.event_source.u8[0], header.event_source.u8[1], header.hops);

  switch (conn->node_role) {
    case NODE_ROLE_CONTROLLER: {
      // Remove header to make packetbuf_dataptr()
      // point to the beginning of the application payload
      if (packetbuf_hdrreduce(sizeof(struct collect_msg_t))) {
        // FIXME Parametri dal payload
        // Call controller callback function
        conn->callbacks->receive_cb(&header.event_source, header.event_seqn,
                                    NULL, 7, 7);
      } else {
        printf("[ETC]: ERROR, header could not be reduced!");
      }

      break;
    }
    case NODE_ROLE_SENSOR_ACTUATOR:
    case NODE_ROLE_FORWARDER: {
      // Detect parent node disconnection
      if (linkaddr_cmp(&conn->parent, &linkaddr_null)) {
        printf(
            "[ETC]: ERROR, unable to forward data packet -- "
            "source: %02x:%02x, hops: %u\n",
            header.event_source.u8[0], header.event_source.u8[1], header.hops);
        return;
      }

      // Update header in the packet buffer
      memcpy(packetbuf_dataptr(), &header, sizeof(header));
      // Send unicast message to parent
      unicast_send(&conn->uc, &conn->parent);

      break;
    }
  }
}

/*---------------------------------------------------------------------------*/
/*                               Command Handling                            */
/*---------------------------------------------------------------------------*/
/* Header structure for data packets */
struct command_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
  /* ... */
} __attribute__((packed));
