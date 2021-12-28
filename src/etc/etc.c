#include "etc.h"

#include <stdio.h>

#include "config/config.h"
#include "connection/beacon/beacon.h"

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
 * @brief Data receive callback.
 *
 * @param uc_conn Unicast connection.
 * @param sender Address of the sender node.
 */
static void unicast_recv(struct unicast_conn *uc_conn,
                         const linkaddr_t *sender);

/* --- Rime Callback structures */

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

  /* Initialize sensors forwarding structure */

  /* Open the underlying Rime primitives */
  beacon_init(conn, channels);
  unicast_open(&conn->uc, channels + 1, &unicast_cb);
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
