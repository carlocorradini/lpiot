#include "connection.h"

#include <net/rime/broadcast.h>
#include <net/rime/unicast.h>

#include "beacon/beacon.h"
#include "logger/logger.h"

const struct connection_t *const best_conn;

/* --- BROADCAST --- */
/**
 * @brief Broadcast connection object.
 */
static struct broadcast_conn bc_conn;

/**
 * @brief Broadcast receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender);

/**
 * @brief Broadcast callback structure.
 */
static const struct broadcast_callbacks bc_cb = {.recv = bc_recv_cb,
                                                 .sent = NULL};

/* --- UNICAST --- */
/**
 * @brief Unicast connection object.
 */
static struct unicast_conn uc_conn;

/* --- --- */
void connection_init(uint16_t channel) {
  /* Open the underlying Rime primitive */
  broadcast_open(&bc_conn, channel, &bc_cb);

  /* Initialize beacon */
  beacon_init(best_conn);
}

bool connection_is_connected() {
  return !linkaddr_cmp(&best_conn->parent_node, &linkaddr_null);
}

int connection_broadcast_send(enum broadcast_msg_type_t type) {
  /* Prepare broadcast header */
  const struct broadcast_hdr_t bc_header = {.type = type};

  if (!connection_is_connected()) return -1; /* Disconnected */
  if (!packetbuf_hdralloc(sizeof(bc_header)))
    return -2; /* Insufficient space */

  /* Copy header */
  memcpy(packetbuf_hdrptr, &bc_header, sizeof(bc_header));

  /* Send */
  return broadcast_send(&bc_conn);
}

static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender) {
  struct broadcast_hdr_t bc_header;

  /* Check received broadcast message validity */
  if (packetbuf_datalen() < sizeof(struct broadcast_hdr_t)) {
    LOG_ERROR("Broadcast message wrong size: %u byte", packetbuf_datalen());
    return;
  }

  /* Copy header */
  memcpy(&bc_header, packetbuf_dataptr(), sizeof(bc_header));

  /* Reduce header in packetbuf */
  if (!packetbuf_hdrreduce(sizeof(struct broadcast_hdr_t))) {
    LOG_ERROR("Error reducing broadcast header");
    return;
  }

  /* Forward to correct callback */
  switch (bc_header.type) {
    case BROADCAST_MSG_TYPE_BEACON: {
      beacon_recv_cb(&bc_header, sender);
      break;
    }
    case BROADCAST_MSG_TYPE_EVENT: {
      /* TODO */
      break;
    }
    default: {
      LOG_WARN("Received broadcast message of unknown type: %d",
               bc_header.type);
      break;
    }
  }
}
