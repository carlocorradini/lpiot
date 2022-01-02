#include "connection.h"

#include <net/mac/mac.h>
#include <net/rime/broadcast.h>
#include <net/rime/unicast.h>

#include "beacon/beacon.h"
#include "logger/logger.h"

/**
 * @brief Connection callbacks pointer.
 */
static const struct connection_callbacks_t *cb;

/* --- BROADCAST --- */
/**
 * @brief Broadcast connection object.
 */
static struct broadcast_conn bc_conn;

/**
 * @brief Send a broadcast message.
 *
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
static bool bc_send(void);

/**
 * @brief Broadcast receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender);

/**
 * @brief Broadcast sent callback.
 *
 * @param bc_conn Broadcast connection.
 * @param status Status code.
 * @param num_tx Total number of transmission(s).
 */
static void bc_sent_cb(struct broadcast_conn *bc_conn, int status, int num_tx);

/**
 * @brief Broadcast callback structure.
 */
static const struct broadcast_callbacks bc_cb = {.recv = bc_recv_cb,
                                                 .sent = bc_sent_cb};

/* --- UNICAST --- */
/**
 * @brief Unicast connection object.
 */
static struct unicast_conn uc_conn;

/**
 * @brief Send a unicast message to receiver address.
 *
 * @param receiver Receiver address.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
static bool uc_send(const linkaddr_t *receiver);

/**
 * @brief Unicast receive callback.
 *
 * @param uc_conn Unicast connection.
 * @param sender Address of the sender node.
 */
static void uc_recv_cb(struct unicast_conn *uc_conn, const linkaddr_t *sender);

/**
 * @brief Unicast sent callback.
 *
 * @param uc_conn Unicast connection.
 * @param status Status code.
 * @param num_tx Total number of transmission(s).
 */
static void uc_sent_cb(struct unicast_conn *uc_conn, int status, int num_tx);

/**
 * @brief Unicast callback structure.
 */
static const struct unicast_callbacks uc_cb = {.recv = uc_recv_cb,
                                               .sent = uc_sent_cb};

/* --- --- */
void connection_open(uint16_t channel,
                     const struct connection_callbacks_t *callbacks) {
  cb = callbacks;

  /* Open the underlying rime primitives */
  broadcast_open(&bc_conn, channel, &bc_cb);
  unicast_open(&uc_conn, channel + 1, &uc_cb);

  /* Initialize beacon */
  beacon_init();
}

void connection_close(void) {
  /* Close the underlying rime primitives */
  broadcast_close(&bc_conn);
  unicast_close(&uc_conn);

  /* Terminate beacon */
  beacon_terminate();
}

bool connection_is_connected(void) {
  return !linkaddr_cmp(&connection_get_conn()->parent_node, &linkaddr_null);
}

const struct connection_t *connection_get_conn(void) {
  return beacon_get_conn();
}

/* --- BROADCAST --- */
static bool bc_send(void) {
  const bool ret = broadcast_send(&bc_conn);

  if (!ret)
    LOG_ERROR("Error sending broadcast message");
  else
    LOG_DEBUG("Sending broadcast message");
  return ret;
}

bool connection_broadcast_send(enum broadcast_msg_type_t type) {
  /* Prepare broadcast header */
  const struct broadcast_hdr_t bc_header = {.type = type};

  /* Allocate header space */
  if (!packetbuf_hdralloc(sizeof(bc_header))) {
    /* Insufficient space */
    LOG_ERROR("Error allocating broadcast header");
    return false;
  }

  /* Copy header */
  memcpy(packetbuf_hdrptr(), &bc_header, sizeof(bc_header));

  /* Send */
  return bc_send();
}

static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender) {
  struct broadcast_hdr_t bc_header;

  /* Check received broadcast message validity */
  if (packetbuf_datalen() < sizeof(bc_header)) {
    LOG_ERROR("Broadcast message from %02x:%02x wrong size: %u byte",
              sender->u8[0], sender->u8[1], packetbuf_datalen());
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
      LOG_DEBUG("Received broadcast message from %02x:%02x of type BEACON",
                sender->u8[0], sender->u8[1]);
      beacon_recv_cb(&bc_header, sender);
      if (cb->bc.recv.beacon != NULL) cb->bc.recv.beacon(&bc_header, sender);
      break;
    }
    case BROADCAST_MSG_TYPE_EVENT: {
      LOG_DEBUG("Received broadcast message from %02x:%02x of type EVENT",
                sender->u8[0], sender->u8[1]);
      if (cb->bc.recv.event != NULL) cb->bc.recv.event(&bc_header, sender);
      break;
    }
    default: {
      LOG_WARN(
          "Received broadcast message from %02x:%02x with unknown type: %d",
          sender->u8[0], sender->u8[1], bc_header.type);
      break;
    }
  }
}

static void bc_sent_cb(struct broadcast_conn *bc_conn, int status, int num_tx) {
  /* Check status */
  if (status != MAC_TX_OK) {
    /* Something bad happended */
    LOG_ERROR("Error sending broadcast message in tx %d due to %d", num_tx,
              status);
    return;
  }

  /* Message sent successfully */
  LOG_DEBUG("Sent broadcast message");
}

/* --- UNICAST --- */
static bool uc_send(const linkaddr_t *receiver) {
  const int ret = unicast_send(&uc_conn, receiver);

  if (!ret)
    LOG_ERROR("Error sending unicast message to %02x:%02x", receiver->u8[0],
              receiver->u8[1]);
  else
    LOG_DEBUG("Sending unicast message to %02x:%02x", receiver->u8[0],
              receiver->u8[1]);
  return ret;
}

bool connection_unicast_send(enum unicast_msg_type_t type,
                             const linkaddr_t *receiver) {
  /* Prepare unicast header */
  const struct unicast_hdr_t uc_header = {.type = type};

  /* Allocate header space */
  if (!packetbuf_hdralloc(sizeof(uc_header))) {
    /* Insufficient space */
    LOG_ERROR("Error allocating unicast header");
    return false;
  }

  /* Copy header */
  memcpy(packetbuf_hdrptr(), &uc_header, sizeof(uc_header));

  /* Send */
  return uc_send(receiver);
}

static void uc_recv_cb(struct unicast_conn *uc_conn, const linkaddr_t *sender) {
  struct unicast_hdr_t uc_header;

  /* Check received unicast message validity */
  if (packetbuf_datalen() < sizeof(uc_header)) {
    LOG_ERROR("Unicast message from %02x:%02x wrong size: %u byte",
              sender->u8[0], sender->u8[1], packetbuf_datalen());
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
    case UNICAST_MSG_TYPE_COLLECT: {
      LOG_DEBUG("Received unicast message from %02x:%02x of type COLLECT",
                sender->u8[0], sender->u8[1]);
      if (cb->uc.recv.collect != NULL) cb->uc.recv.collect(&uc_header, sender);
      break;
    }
    default: {
      LOG_WARN("Received unicast message from %02x:%02x with unknown type: %d",
               sender->u8[0], sender->u8[1], uc_header.type);
      break;
    }
  }
}

static void uc_sent_cb(struct unicast_conn *uc_conn, int status, int num_tx) {
  /* Receiver address */
  const linkaddr_t *receiver = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  /* Current connection  */
  const struct connection_t *conn = connection_get_conn();

  /* Ignore if null address */
  if (linkaddr_cmp(receiver, &linkaddr_null)) {
    LOG_WARN("Unicast message sent to NULL address %02x:%02x", receiver->u8[0],
             receiver->u8[1]);
    return;
  }

  /* Check status */
  if (status != MAC_TX_OK) {
    /* Something bad happended */
    LOG_ERROR("Error sending unicast message to %02x:%02x in tx %d due to %d",
              receiver->u8[0], receiver->u8[1], num_tx, status);

    /* Ignore backup if receiver is not parent node */
    if (!linkaddr_cmp(receiver, &conn->parent_node)) return;

    /* Invalidate current connection */
    LOG_WARN(
        "Invalidating connection: { parent_node: %02x:%02x, seqn: %u, "
        "hopn: %u, rssi: %d }",
        conn->parent_node.u8[0], conn->parent_node.u8[1], conn->seqn,
        conn->hopn, conn->rssi);
    beacon_invalidate_connection();

    /* Check if backup connection is available */
    if (!connection_is_connected()) {
      LOG_WARN("Unavailable backup connection");
      return;
    }

    /* New connection */
    const struct connection_t *new_conn = connection_get_conn();
    LOG_INFO(
        "Available backup connection: { parent_node: %02x:%02x, seqn: %u, "
        "hopn: %u, rssi: %d }",
        new_conn->parent_node.u8[0], new_conn->parent_node.u8[1],
        new_conn->seqn, new_conn->hopn, new_conn->rssi);

    /* Retry */
    linkaddr_t data_receiver;
    uint8_t data[PACKETBUF_SIZE];
    uint16_t data_len;
    struct unicast_hdr_t data_header;

    linkaddr_copy(&data_receiver, receiver);
    packetbuf_copyto(&data);
    data_len = packetbuf_datalen();
    /* FIXME Non funziona */
    memcpy(&data_header, packetbuf_dataptr() - sizeof(data_header),
           sizeof(data_header));

    packetbuf_clear();
    packetbuf_copyfrom(&data, data_len);

    connection_unicast_send(data_header.type, &new_conn->parent_node);

    return;
  }

  /* Message sent successfully */
  LOG_DEBUG("Sent unicast message to %02x:%02x", receiver->u8[0],
            receiver->u8[1]);
}
