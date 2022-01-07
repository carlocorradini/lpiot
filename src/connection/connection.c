#include "connection.h"

#include <net/mac/mac.h>
#include <net/rime/broadcast.h>
#include <net/rime/unicast.h>

#include "beacon.h"
#include "config/config.h"
#include "connection/uc_buffer.h"
#include "forward.h"
#include "logger/logger.h"
#include "node/node.h"

/**
 * @brief Connection callbacks pointer.
 */
static const struct connection_callbacks_t *cb;

/**
 * @brief Forward discovery message.
 */
struct forward_discovery_msg_t {
  /* Sensor address. */
  linkaddr_t sensor;
  /* Hop distance. */
  uint8_t distance;
};

/**
 * @brief Invalidate first hop of the sensor.
 *
 * @param sensor Sensor address.
 * @return true New hop available.
 * @return false No hop available.
 */
static bool invalidate_hop(const linkaddr_t *sensor);

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
 * @param uc_header Header.
 * @param receiver Receiver address.
 * @return true Message sent.
 * @return false Message not sent due to an error.
 */
static bool uc_send(const struct unicast_hdr_t *uc_header,
                    const linkaddr_t *receiver);

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
 * @brief Send next message in buffer (if any).
 */
static void uc_send_next(void);

/**
 * @brief Unicast callback structure.
 */
static const struct unicast_callbacks uc_cb = {.recv = uc_recv_cb,
                                               .sent = uc_sent_cb};

/* --- FORWARD DISCOVERY --- */
/**
 * @brief Forward discovery timer.
 */
static struct ctimer forward_discovery_timer;

/**
 * @brief Broadcast receive callback for a forward discovery message.
 *
 * @param bc_header Broadcast header.
 * @param sender Sender address.
 */
static void forward_discovery_recv_cb(const struct broadcast_hdr_t *bc_header,
                                      const linkaddr_t *sender);

/**
 * @brief Forward delivery timer callback.
 *
 * @param ignored
 */
static void forward_discovery_timer_cb(void *ignored);

/* --- --- */
void connection_open(uint16_t channel,
                     const struct connection_callbacks_t *callbacks) {
  cb = callbacks;

  /* Initialize unicast buffer */
  uc_buffer_init();

  /* Initialize forward structure */
  forward_init();

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

  /* Terminate forward structure */
  forward_terminate();

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

bool connection_invalidate(void) {
  if (!connection_is_connected()) return false;

  /* Obatin current connection */
  const struct connection_t *conn = connection_get_conn();

  LOG_WARN(
      "Invalidating connection: "
      "{ parent_node: %02x:%02x, seqn: %u, hopn: %u, rssi: %d }",
      conn->parent_node.u8[0], conn->parent_node.u8[1], conn->seqn, conn->hopn,
      conn->rssi);

  /* Invalidate */
  beacon_invalidate_connection();

  /* Check if backup connection is available */
  if (!connection_is_connected()) {
    LOG_WARN("Backup connection not available");
    return false;
  }

  /* New connection */
  const struct connection_t *new_conn = connection_get_conn();
  LOG_INFO(
      "Backup connection: "
      "{ parent_node: %02x:%02x, seqn: %u, hopn: %u, rssi: %d }",
      new_conn->parent_node.u8[0], new_conn->parent_node.u8[1], new_conn->seqn,
      new_conn->hopn, new_conn->rssi);

  return true;
}

static bool invalidate_hop(const linkaddr_t *sensor) {
  /* Find a forward node */
  const struct forward_t *forward = forward_find(sensor);
  if (forward == NULL) return false;

  if (forward_hops_length(sensor) != 0) {
    LOG_WARN(
        "Invalidating hop for sensor %02x:%02x: "
        "{ address: %02x:%02x, distance: %u }",
        sensor->u8[0], sensor->u8[1], forward->hops[0].address.u8[0],
        forward->hops[0].address.u8[1], forward->hops[0].distance);
  }

  /* Remove broken forward for final receiver */
  forward_remove_first(sensor);

  /* New forward node */
  const struct forward_t *new_forward = forward_find(sensor);
  if (new_forward == NULL) return false;

  /* Check no hop available */
  if (forward_hops_length(&new_forward->sensor) == 0) {
    LOG_WARN("No hop available for sensor %02x:%02x", sensor->u8[0],
             sensor->u8[1]);
    return false;
  }

  /* Hop available */
  LOG_INFO(
      "Available backup hop for sensor %02x:%02x: "
      "{ address: %02x:%02x, distance: %u }",
      sensor->u8[0], sensor->u8[1], new_forward->hops[0].address.u8[0],
      new_forward->hops[0].address.u8[1], new_forward->hops[0].distance);
  return true;
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

  switch (bc_header.type) {
    case BROADCAST_MSG_TYPE_BEACON: {
      /* Forward to beacon */
      beacon_recv_cb(&bc_header, sender);
      break;
    }
    case BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_REQUEST:
    case BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_RESPONSE: {
      /* Forward to forward discovery */
      forward_discovery_recv_cb(&bc_header, sender);
      break;
    }
    default: {
      /* Forward to callback */
      if (cb->bc.recv != NULL) cb->bc.recv(&bc_header, sender);
      break;
    }
  }
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
static bool uc_send(const struct unicast_hdr_t *uc_header,
                    const linkaddr_t *receiver) {
  /* Allocate header space */
  if (!packetbuf_hdralloc(sizeof(struct unicast_hdr_t))) {
    /* Insufficient space */
    LOG_ERROR("Error allocating unicast header");
    return false;
  }

  /* Copy header */
  memcpy(packetbuf_hdrptr(), uc_header, sizeof(struct unicast_hdr_t));

  /* Send */
  const bool ret = unicast_send(&uc_conn, receiver);

  if (!ret) {
    LOG_ERROR(
        "Error sending unicast message to %02x:%02x: { type: %d, hops: %u }",
        receiver->u8[0], receiver->u8[1], uc_header->type, uc_header->hops);
    /* Remove message from buffer */
    uc_buffer_remove();
  } else {
    LOG_DEBUG("Sending unicast message to %02x:%02x: { type: %d, hops: %u }",
              receiver->u8[0], receiver->u8[1], uc_header->type,
              uc_header->hops);
    /* Increase send counter */
    uc_buffer_first()->num_send += 1;
  }
  return ret;
}

bool connection_unicast_send(const struct unicast_hdr_t *uc_header,
                             const linkaddr_t *receiver) {
  /* Check if null address */
  if (linkaddr_cmp(receiver, &linkaddr_null)) {
    LOG_WARN("Unable to send unicast message: NULL address: %02x:%02x",
             receiver->u8[0], receiver->u8[1]);
    return false;
  }

  /* Check connection only if receiver is parent node */
  if (linkaddr_cmp(receiver, &connection_get_conn()->parent_node) &&
      !connection_is_connected()) {
    LOG_WARN("Unable to send unicast message: No connection available");
    return false;
  }

  /* Add to buffer */
  if (!uc_buffer_add(uc_header, receiver)) {
    LOG_ERROR(
        "Unicast buffer is full, message of type %d to %02x:%02x not sent",
        uc_header->type, receiver->u8[0], receiver->u8[1]);
    return false;
  }

  /* Start sending if first in buffer */
  if (uc_buffer_length() - 1 == 0) return uc_send(uc_header, receiver);

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

  /* Increase hop counter */
  uc_header.hops += 1;

  LOG_DEBUG("Received unicast message from %02x:%02x: { type: %d, hops: %d }",
            sender->u8[0], sender->u8[1], uc_header.type, uc_header.hops);

  /* Check hop counter */
  if (uc_header.hops >= CONNECTION_MAX_HOPS) {
    LOG_WARN(
        "Received unicast message has reached the maximum number of hops "
        "allowed: %u/%u",
        uc_header.hops, CONNECTION_MAX_HOPS);
    return;
  }

  /* Check loops */
  switch (uc_header.type) {
    case UNICAST_MSG_TYPE_COLLECT: {
      /* Check sender is not parent node */
      if (linkaddr_cmp(sender, &connection_get_conn()->parent_node)) {
        LOG_WARN(
            "Loop detected: Received collect message from parent node "
            "%02x:%02x",
            sender->u8[0], sender->u8[1]);
        /* Invalidate connection */
        connection_invalidate();
      }
      break;
    }
    case UNICAST_MSG_TYPE_COMMAND: {
      const struct forward_t *forward = forward_find(&uc_header.final_receiver);

      /* Check sender is not hop */
      if (linkaddr_cmp(sender, &forward->hops[0].address)) {
        LOG_WARN(
            "Loop detected: Received command message from hop "
            "%02x:%02x",
            sender->u8[0], sender->u8[1]);
        /* Invalidate hop */
        invalidate_hop(&uc_header.final_receiver);
      }
      break;
    }
  }

  /* Forward to callback */
  if (cb->uc.recv != NULL) cb->uc.recv(&uc_header, sender);
}

static void uc_sent_cb(struct unicast_conn *uc_conn, int status, int num_tx) {
  /* Receiver address */
  const linkaddr_t *receiver = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  /* Obtain buffered message */
  struct uc_buffer_t *message = uc_buffer_first();

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

    /* Retry ? */
    bool retry = message->num_send < CONNECTION_UC_BUFFER_MAX_SEND;

    /* Logic */
    if (!retry) {
      switch (message->header.type) {
        case UNICAST_MSG_TYPE_COLLECT: {
          /* Ignore if receiver is not parent */
          if (!message->receiver_is_parent) break;

          /* Give a last chance if receiver is controller */
          if (!message->last_chance && linkaddr_cmp(receiver, &CONTROLLER)) {
            retry = true;
            message->last_chance = true;
            break;
          }

          /* Receiver is current parent */
          if (linkaddr_cmp(receiver, &conn->parent_node)) {
            /* Invalidate connection */
            if (connection_invalidate()) {
              retry = true;
              message->last_chance = false;
              message->num_send = CONNECTION_UC_BUFFER_MAX_SEND - 1;
            }
            break;
          } else {
            /* Parent changed dinamically */
            retry = true;
            message->last_chance = false;
            message->num_send = CONNECTION_UC_BUFFER_MAX_SEND - 1;
            break;
          }

          break;
        }
        case UNICAST_MSG_TYPE_COMMAND: {
          /* Ignore if handle NULL */
          if (linkaddr_cmp(&message->header.final_receiver, &linkaddr_null))
            break;

          /* Give a last chance */
          if (!message->last_chance) {
            retry = true;
            message->last_chance = true;
            break;
          }

          /* Invalidate hop */
          invalidate_hop(&message->header.final_receiver);

          /* Try with new hop or prepare to discovery */
          retry = true;
          message->num_send = CONNECTION_UC_BUFFER_MAX_SEND - 1;

          break;
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
    if (cb->uc.sent != NULL) cb->uc.sent(true);
  }

  uc_send_next();
}

static void uc_send_next(void) {
  /* Send message in buffer (if any) */
  while (!uc_bufffer_is_empty()) {
    /* Obtain buffered message */
    struct uc_buffer_t *message = uc_buffer_first();

    /* No last chance and maximum number of send */
    if (!message->last_chance &&
        message->num_send >= CONNECTION_UC_BUFFER_MAX_SEND) {
      LOG_WARN(
          "Buffered message could not be sent because has reached the maximum "
          "number of send: { receiver: %02x:%02x, type: %d }",
          message->receiver.u8[0], message->receiver.u8[1],
          message->header.type);
      /* Remove entry */
      uc_buffer_remove();
      /* Forward to callback */
      if (cb->uc.sent != NULL) cb->uc.sent(false);
      continue;
    }

    /* Logic */
    switch (message->header.type) {
      case UNICAST_MSG_TYPE_COLLECT: {
        /* If disconnected no collect message could be sent */
        if (!connection_is_connected()) {
          LOG_WARN(
              "Node disconnected, buffered message could not be sent: "
              "{ receiver: %02x:%02x, type: %d }",
              message->receiver.u8[0], message->receiver.u8[1],
              message->header.type);
          /* Remove entry */
          uc_buffer_remove();
          /* Forward to callback */
          if (cb->uc.sent != NULL) cb->uc.sent(false);
          continue;
        }

        /* Connection available, update receiver (parent node) */
        linkaddr_copy(&message->receiver, &connection_get_conn()->parent_node);
        break;
      }
      case UNICAST_MSG_TYPE_COMMAND: {
        const struct forward_t *forward =
            forward_find(&message->header.final_receiver);
        if (forward == NULL) break;

        /* If no available hop try to find one */
        if (forward_hops_length(&forward->sensor) == 0) {
          /* Hop not available */
          LOG_WARN("No hop available: try to find one...");

          /* Prepare forward discovery message */
          struct forward_discovery_msg_t fd_msg;
          linkaddr_copy(&fd_msg.sensor, &forward->sensor);
          fd_msg.distance = UINT8_MAX;

          /* Try to discover a forward node */
          packetbuf_clear();
          packetbuf_copyfrom(&fd_msg, sizeof(fd_msg));

          /* Send */
          if (!bc_send(BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_REQUEST)) {
            LOG_ERROR(
                "Error sending forward discovery request message for sensor "
                "%02x:%02x",
                forward->sensor.u8[0], forward->sensor.u8[1]);
            /* Remove entry */
            uc_buffer_remove();
            /* Forward to callback */
            if (cb->uc.sent != NULL) cb->uc.sent(false);
            continue;
          }

          /* Sent */
          LOG_INFO(
              "Sending forward discovery request message for sensor "
              "%02x:%02x",
              forward->sensor.u8[0], forward->sensor.u8[1]);
          /* Start forward discovery timeout */
          ctimer_set(&forward_discovery_timer,
                     CONNECTION_FORWARD_DISCOVERY_TIMEOUT,
                     forward_discovery_timer_cb, NULL);
          return;
        }

        /* Hop available, update receiver (hop node) */
        linkaddr_copy(&message->receiver, &forward->hops[0].address);
        break;
      }
      default: {
        /* Ignore */
        break;
      }
    }

    /* Prepare packetbuf */
    packetbuf_clear();
    packetbuf_copyfrom(message->data, message->data_len);

    /* Send */
    if (!uc_send(&message->header, &message->receiver)) {
      LOG_ERROR("Error sending buffered mesage");
      /* Forward to callback */
      if (cb->uc.sent != NULL) cb->uc.sent(false);
      continue;
    }

    /* Sent */
    LOG_INFO(
        "Sending buffered unicast message: "
        "{ receiver: %02x:%02x, type: %d, num_send: %u }",
        message->receiver.u8[0], message->receiver.u8[1], message->header.type,
        message->num_send);
    return;
  }
}

/* --- FORWARD DISCOVERY --- */
static void forward_discovery_recv_cb(const struct broadcast_hdr_t *bc_header,
                                      const linkaddr_t *sender) {
  struct forward_discovery_msg_t fd_msg;

  /* Check received request message */
  if (packetbuf_datalen() != sizeof(fd_msg)) {
    LOG_ERROR(
        "Received forward discovery message of type %d wrong size: %u byte",
        bc_header->type, packetbuf_datalen());
    return;
  }

  /* Copy request message */
  packetbuf_copyto(&fd_msg);

  LOG_INFO(
      "Received forward discovery message of type %d from %02x:%02x: "
      "{ sensor: %02x:%02x, distance: %u }",
      bc_header->type, sender->u8[0], sender->u8[1], fd_msg.sensor.u8[0],
      fd_msg.sensor.u8[1], fd_msg.distance);

  /* Logic */
  switch (bc_header->type) {
    case BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_REQUEST: {
      /* Remove forward hop for sensor (if exists) */
      forward_remove_hop(&fd_msg.sensor, sender);

      /* Ignore if sensor is not me and not known */
      if (!linkaddr_cmp(&fd_msg.sensor, &linkaddr_node_addr) &&
          forward_hops_length(&fd_msg.sensor) == 0) {
        LOG_WARN("Forward for sensor %02x:%02x is not known",
                 fd_msg.sensor.u8[0], fd_msg.sensor.u8[1]);
        return;
      }

      if (linkaddr_cmp(&fd_msg.sensor, &linkaddr_node_addr)) {
        /* Sensor is me, set distance to 0 */
        fd_msg.distance = 0;
      } else {
        /* Known, set distance to value */
        fd_msg.distance = forward_find(&fd_msg.sensor)->hops[0].distance;
      }

      LOG_INFO(
          "Forward for sensor %02x:%02x is known with a distance of %u hops",
          fd_msg.sensor.u8[0], fd_msg.sensor.u8[1], fd_msg.distance);

      /* Prepare packetbuf */
      packetbuf_clear();
      packetbuf_copyfrom(&fd_msg, sizeof(fd_msg));

      /* Send */
      if (!bc_send(BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_RESPONSE)) {
        LOG_ERROR(
            "Error sending forward discovery response message for sensor "
            "%02x:%02x to %02x:%02x",
            fd_msg.sensor.u8[0], fd_msg.sensor.u8[1], sender->u8[0],
            sender->u8[1]);
        break;
      }

      /* Sent */
      LOG_INFO(
          "Sending forward discovery response message for sensor "
          "%02x:%02x to %02x:%02x",
          fd_msg.sensor.u8[0], fd_msg.sensor.u8[1], sender->u8[0],
          sender->u8[1]);
      break;
    }
    case BROADCAST_MSG_TYPE_FORWARD_DISCOVERY_RESPONSE: {
      /* Learn */
      forward_add(&fd_msg.sensor, sender, fd_msg.distance);

      /* Ignore if timer expired */
      if (ctimer_expired(&forward_discovery_timer)) {
        LOG_WARN(
            "Ignoring forward discovery response from %02x:%02x beacuse the "
            "timer has expired",
            sender->u8[0], sender->u8[1]);
        return;
      }

      /* Original sensor address */
      const linkaddr_t *sensor = &uc_buffer_first()->header.final_receiver;

      /* Check request and response match */
      if (!linkaddr_cmp(sensor, &fd_msg.sensor)) {
        LOG_WARN(
            "Forward discovery response has wrong sensor: request is %02x:%02x "
            "but response is %02x:%02x",
            sensor->u8[0], sensor->u8[1], fd_msg.sensor.u8[0],
            fd_msg.sensor.u8[1]);
        return;
      }

      /* Add forward */
      LOG_INFO(
          "Forward discovery response from %02x:%02x: "
          "{ address: %02x:%02x, distance: %u }",
          sender->u8[0], sender->u8[1], fd_msg.sensor.u8[0],
          fd_msg.sensor.u8[1], fd_msg.distance);
      break;
    }
    default: {
      /* Ignored */
      break;
    }
  }
}

static void forward_discovery_timer_cb(void *ignored) {
  const struct forward_t *forward =
      forward_find(&uc_buffer_first()->header.final_receiver);
  const size_t hops_length = forward_hops_length(&forward->sensor);

  LOG_INFO("Forward discovery timer expired for sensor %02x:%02x",
           forward->sensor.u8[0], forward->sensor.u8[1]);
  LOG_INFO("Available hops for sensor %02x:%02x: %d", forward->sensor.u8[0],
           forward->sensor.u8[1], hops_length);

  if (hops_length == 0) {
    LOG_WARN("Forward discovery failed");
    /* Remove entry */
    uc_buffer_remove();
    /* Forward to callback */
    if (cb->uc.sent != NULL) cb->uc.sent(false);
  } else {
    LOG_INFO("Forward discovery succeeded");
  }

  /* Next unicast buffer */
  uc_send_next();
}
