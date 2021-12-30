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

/**
 * @brief Unicast receive callback.
 *
 * @param uc_conn Unicast connection.
 * @param sender Address of the sender node.
 */
static void uc_recv_cb(struct unicast_conn *uc_conn, const linkaddr_t *sender);

/**
 * @brief Unicast callback structure.
 */
static const struct unicast_callbacks uc_cb = {.recv = uc_recv_cb,
                                               .sent = NULL};

/* --- --- */
void connection_init(uint16_t channel) {
  /* Open the underlying rime primitive */
  broadcast_open(&bc_conn, channel, &bc_cb);
  unicast_open(&uc_conn, channel + 1, &uc_cb);

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
  memcpy(packetbuf_hdrptr(), &bc_header, sizeof(bc_header));

  /* Send */
  const int ret = broadcast_send(&bc_conn);
  if (!ret)
    LOG_DEBUG("Error sending broadcast message");
  else
    LOG_DEBUG("Broadcast message sent successfully");
  return ret;
}

static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender) {
  struct broadcast_hdr_t bc_header;

  /* Check received broadcast message validity */
  if (packetbuf_datalen() < sizeof(bc_header)) {
    LOG_ERROR("Broadcast message wrong size: %u byte", packetbuf_datalen());
    return;
  }

  /* Copy header */
  memcpy(&bc_header, packetbuf_dataptr(), sizeof(bc_header));

  /* Reduce header in packetbuf */
  if (!packetbuf_hdrreduce(sizeof(bc_header))) {
    LOG_ERROR("Error reducing broadcast header");
    return;
  }

  /* Forward to correct callback */
  switch (bc_header.type) {
    case BROADCAST_MSG_TYPE_BEACON: {
      LOG_DEBUG("Received broadcast message of type BEACON");
      beacon_recv_cb(&bc_header, sender);
      break;
    }
    case BROADCAST_MSG_TYPE_EVENT: {
      LOG_DEBUG("Received broadcast message of type EVENT");
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

static void uc_recv_cb(struct unicast_conn *uc_conn, const linkaddr_t *sender) {
  struct unicast_hdr_t uc_header;

  /* Check received unicast message validity */
  if (packetbuf_datalen() < sizeof(uc_header)) {
    LOG_ERROR("Unicast message wrong size: %u byte", packetbuf_datalen());
    return;
  }

  /* Copy header */
  memcpy(&uc_header, packetbuf_dataptr(), sizeof(uc_header));

  /* Reduce header in packetbuf */
  if (!packetbuf_hdrreduce(sizeof(uc_header))) {
    LOG_ERROR("Error reducing unicast header");
    return;
  }

  /* Forward to correct callback */
  switch (uc_header.type) {
    default: {
      LOG_WARN("Received unicast message of unknown type: %d", uc_header.type);
      break;
    }
  }
}
