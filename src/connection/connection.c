#include "connection.h"

#include <net/mac/mac.h>
#include <net/rime/broadcast.h>
#include <net/rime/unicast.h>

#include "beacon/beacon.h"
#include "config/config.h"
#include "connection/uc_buffer.h"
#include "logger/logger.h"
#include "node/node.h"

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
 * @param type Message type.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
static bool bc_send(enum broadcast_msg_type_t type);

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
 * @param num_tx Number of transmission(s).
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
 * @param type Message type.
 * @param receiver Receiver address.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
static bool uc_send(enum unicast_msg_type_t type, const linkaddr_t *receiver);

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

  /* Initialize unicast buffer */
  uc_buffer_init();

  /* Open the underlying rime primitives */
  broadcast_open(&bc_conn, channel, &bc_cb);
  unicast_open(&uc_conn, channel + 1, &uc_cb);

  /* Initialize beacon */
  beacon_init();
}

void connection_close(void) {
  cb = NULL;

  /* Terminate unicast buffer */
  uc_buffer_terminate();

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
static bool bc_send(enum broadcast_msg_type_t type) {
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
  const bool ret = broadcast_send(&bc_conn);
  if (!ret)
    LOG_ERROR("Error sending broadcast message");
  else
    LOG_DEBUG("Sending broadcast message");
  return ret;
}

bool connection_broadcast_send(enum broadcast_msg_type_t type) {
  /* Send */
  return bc_send(type);
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

  LOG_DEBUG("Received broadcast message from %02x:%02x of type %d",
            sender->u8[0], sender->u8[1], bc_header.type);

  /* Forward to beacon */
  if (bc_header.type == BROADCAST_MSG_TYPE_BEACON)
    beacon_recv_cb(&bc_header, sender);

  /* Forward to callback */
  if (cb->bc.recv != NULL) cb->bc.recv(&bc_header, sender);
}

static void bc_sent_cb(struct broadcast_conn *bc_conn, int status, int num_tx) {
  if (status != MAC_TX_OK)
    LOG_ERROR("Error sending broadcast message on tx %d due to %d", num_tx,
              status); /* Something bad happended */
  else
    LOG_DEBUG("Sent broadcast message"); /* Message sent */

  /* Forward to callback */
  if (cb->bc.sent != NULL) cb->bc.sent(status, num_tx);
}

/* --- UNICAST --- */
static bool uc_send(enum unicast_msg_type_t type, const linkaddr_t *receiver) {
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
  const bool ret = unicast_send(&uc_conn, receiver);
  uc_buffer_first()->num_send += 1;
  if (!ret) {
    LOG_ERROR("Error sending unicast message to %02x:%02x", receiver->u8[0],
              receiver->u8[1]);
    /* Remove message from buffer */
    uc_buffer_remove();
  } else
    LOG_DEBUG("Sending unicast message to %02x:%02x", receiver->u8[0],
              receiver->u8[1]);
  return ret;
}

bool connection_unicast_send(enum unicast_msg_type_t type,
                             const linkaddr_t *receiver) {
  /* Check if null address */
  if (linkaddr_cmp(receiver, &linkaddr_null)) {
    LOG_WARN("Unable to send unicast message to NULL address: %02x:%02x",
             receiver->u8[0], receiver->u8[1]);
    return false;
  }

  /* Check connection only if receiver is parent node */
  if (linkaddr_cmp(receiver, &connection_get_conn()->parent_node) &&
      !connection_is_connected()) {
    LOG_WARN("Error sending unicast message: No connection available");
    return false;
  }

  /* Add to buffer */
  if (!uc_buffer_add(type, receiver)) {
    LOG_ERROR(
        "Unicast buffer is full, message of type %d to %02x:%02x not sent",
        type, receiver->u8[0], receiver->u8[1]);
    return false;
  }

  /* Start sending if first in buffer */
  if (uc_buffer_length() - 1 == 0) return uc_send(type, receiver);

  return true;
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

  LOG_DEBUG("Received unicast message from %02x:%02x of type %d", sender->u8[0],
            sender->u8[1], uc_header.type);

  /* Forward to callback */
  if (cb->uc.recv != NULL) cb->uc.recv(&uc_header, sender);
}

static void uc_sent_cb(struct unicast_conn *uc_conn, int status, int num_tx) {
  /* Receiver address */
  const linkaddr_t *receiver = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  /* Check if null address */
  if (linkaddr_cmp(receiver, &linkaddr_null)) {
    LOG_WARN("Unicast message sent to NULL address %02x:%02x", receiver->u8[0],
             receiver->u8[1]);
  } else if (status != MAC_TX_OK) {
    /* Something bad happended */
    LOG_ERROR("Error sending unicast message to %02x:%02x on tx %d due to %d",
              receiver->u8[0], receiver->u8[1], num_tx, status);
    /* Current connection  */
    const struct connection_t *conn = connection_get_conn();
    /* Obtain buffered message */
    struct uc_buffer_t *message = uc_buffer_first();

    /* Retry ? */
    bool retry = message->num_send < CONNECTION_UC_BUFFER_MAX_SEND;

    /* Backup parent connection if receiver is parent node */
    if (!retry && linkaddr_cmp(receiver, &conn->parent_node)) {
      /* Give a last chance if receiver is controller */
      if (linkaddr_cmp(receiver, &CONTROLLER) && !message->last_chance) {
        /* Retry and give last chance */
        retry = true;
        message->last_chance = true;
      } else {
        /* Invalidate current connection */
        LOG_WARN(
            "Invalidating connection: "
            "{ parent_node: %02x:%02x, seqn: %u, hopn: %u, rssi: %d }",
            conn->parent_node.u8[0], conn->parent_node.u8[1], conn->seqn,
            conn->hopn, conn->rssi);
        beacon_invalidate_connection();

        /* Check if backup connection is available */
        if (!connection_is_connected()) {
          LOG_WARN("Unavailable backup connection");
        } else {
          /* New connection */
          const struct connection_t *new_conn = connection_get_conn();
          LOG_INFO(
              "Available backup connection: "
              "{ parent_node: %02x:%02x, seqn: %u, hopn: %u, rssi: %d }",
              new_conn->parent_node.u8[0], new_conn->parent_node.u8[1],
              new_conn->seqn, new_conn->hopn, new_conn->rssi);

          /* Retry and give last chance */
          retry = true;
          message->last_chance = true;
        }
      }
    }

    if (!retry) {
      message->last_chance = false;
    } else {
      LOG_INFO("Retrying to send last unicast message");
    }
  } else {
    /* Message sent successfully */
    LOG_DEBUG("Sent unicast message to %02x:%02x", receiver->u8[0],
              receiver->u8[1]);
    /* Remove entry */
    uc_buffer_remove();
    /* Forward to callback */
    if (cb->uc.sent != NULL) cb->uc.sent(status, num_tx);
  }

  /* Send message in buffer (if any) */
  while (!uc_bufffer_is_empty()) {
    /* Obtain buffered message */
    struct uc_buffer_t *message = uc_buffer_first();

    /* Check if no retries left */
    if (!message->last_chance &&
        message->num_send >= CONNECTION_UC_BUFFER_MAX_SEND) {
      LOG_WARN(
          "Buffered message could not be sent because has reached the maximum "
          "number of send: { receiver: %02x:%02x, type: %d }",
          message->receiver.u8[0], message->receiver.u8[1], message->msg_type);
      /* Remove entry */
      uc_buffer_remove();
      /* Forward to callback */
      if (cb->uc.sent != NULL) cb->uc.sent(status, num_tx);
      continue;
    }

    switch (message->msg_type) {
      case UNICAST_MSG_TYPE_COLLECT: {
        /* If disconnected no collect message could be sent */
        if (!connection_is_connected()) {
          LOG_WARN(
              "Node disconnected, buffered message could not be sent: "
              "{ receiver: %02x:%02x, type: %d }",
              message->receiver.u8[0], message->receiver.u8[1],
              message->msg_type);
          /* Remove entry */
          uc_buffer_remove();
          /* Forward to callback */
          if (cb->uc.sent != NULL) cb->uc.sent(MAC_TX_OK, num_tx);
          continue;
        }
        /* Connection available, update receiver (parent node) */
        linkaddr_copy(&message->receiver, &connection_get_conn()->parent_node);
        break;
      }
    }

    /* Prepare packetbuf */
    packetbuf_clear();
    packetbuf_copyfrom(message->data, message->data_len);

    /* Send */
    if (!uc_send(message->msg_type, &message->receiver)) {
      LOG_ERROR("Error sending buffered mesage");
      /* Forward to callback */
      if (cb->uc.sent != NULL) cb->uc.sent(false, num_tx);
      continue;
    }
    LOG_INFO(
        "Sending buffered unicast message: "
        "{ receiver: %02x:%02x, type: %d, num_send: %u }",
        message->receiver.u8[0], message->receiver.u8[1], message->msg_type,
        message->num_send);
    break;
  }
}
