#include "etc.h"

#include <net/mac/mac.h>

#include "config/config.h"
#include "net/packetbuf.h"

/**
 * @brief Event message.
 */
struct event_msg_t {
  /* Address of the sensor that generated the event. */
  linkaddr_t source;
  /* Event sequence number. */
  uint16_t seqn;
} __attribute__((packed));

/**
 * @brief Header structure for data packets.
 */
struct collect_msg_t {
  /* Address of the sensor that generated the event. */
  linkaddr_t event_source;
  /* Event sequence number. */
  uint16_t event_seqn;
} __attribute__((packed));

/**
 * @brief Header structure for command packets.
 */
struct command_msg_t {
  /* Address of the sensor that generated the event. */
  linkaddr_t event_source;
  /* Event sequence number. */
  uint16_t event_seqn;
} __attribute__((packed));

/**
 * @brief ETC callback(s) to interact with the node.
 */
static const struct etc_callbacks_t *cb;

/**
 * @brief Current event.
 */
static struct etc_event_t event;

/**
 * @brief Timer to stop the generation of new event(s).
 */
static struct ctimer suppression_timer_new;

/**
 * @brief Timer to stop the propagation of event(s).
 */
static struct ctimer suppression_timer_propagation;

/**
 * @brief Timer to wait before sending the event message.
 */
static struct ctimer event_timer;

/**
 * @brief Timer to wait before sending the collect message.
 */
static struct ctimer collect_timer;

/* --- EVENT MESSAGE--- */
/**
 * @brief Event message receive callback.
 *
 * @param header Broadcast header.
 * @param sender Address of the sender node.
 */
static void event_msg_cb(const struct broadcast_hdr_t *header,
                         const linkaddr_t *sender);

/**
 * @brief Event timer callback.
 *
 * @param ignored
 */
static void event_timer_cb(void *ignored);

/**
 * @brief Send event message.
 *
 * @param event_msg Event message to send.
 * @return true Event message sent.
 * @return false Event message not sent due to an error.
 */
static bool send_event_message(const struct event_msg_t *event_msg);

/* --- COLLECT MESSAGE --- */
static bool sending_collect_msg = false;

/**
 * @brief Collect message receive callback.
 *
 * @param header Unicast header.
 * @param sender Address of the sender node.
 */
static void collect_msg_cb(const struct unicast_hdr_t *header,
                           const linkaddr_t *sender);

/**
 * @brief Collect timer callback.
 *
 * @param ignored
 */
static void collect_timer_cb(void *ignored);

/**
 * @brief Send collect message to receiver node.
 * The final recipient of the collect must be the Controller node.
 *
 * @param collect_msg Collect message to send.
 * @param receiver Receiver node address.
 * @return true Collect message sent.
 * @return false Collect message not sent due to an error.
 */
static bool send_collect_message(const struct collect_msg_t *collect_msg,
                                 const linkaddr_t *receiver);

/* --- COMMAND MESSAGE--- */
/**
 * @brief Command message receive callback.
 *
 * @param header Unicast header.
 * @param sender Address of the sender node.
 */
static void command_msg_cb(const struct unicast_hdr_t *header,
                           const linkaddr_t *sender);

/* --- CONNECTION --- */
/* --- Broadcast */
/**
 * @brief Broadcast receive callback.
 *
 * @param header Broadcast header.
 * @param sender Address of the sender node.
 */
static void bc_recv(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender);

/* --- Unicast */
/**
 * @brief Unicast receive callback.
 *
 * @param header Unicast header.
 * @param sender Address of the sender node.
 */
static void uc_recv(const struct unicast_hdr_t *header,
                    const linkaddr_t *sender);

/**
 * @brief Unicast sent callback.
 *
 * @param status Status code.
 * @param num_tx Number of transmission(s).
 */
static void uc_sent(int status, int num_tx);

/**
 * @brief Connection callbacks.
 */
static struct connection_callbacks_t conn_cb = {
    .bc = {.recv = bc_recv, .sent = NULL},
    .uc = {.recv = uc_recv, .sent = uc_sent}};

/* --- --- */
void etc_open(uint16_t channel, const struct etc_callbacks_t *callbacks) {
  cb = callbacks;

  /* Initialize event */
  event.seqn = 0;
  linkaddr_copy(&event.source, &linkaddr_null);

  /* Open connection */
  connection_open(channel, &conn_cb);
}

void etc_close(void) {
  /* Reset */
  cb = NULL;
  event.seqn = 0;
  linkaddr_copy(&event.source, &linkaddr_null);

  /* Close connection */
  connection_close();
}

const struct etc_event_t *etc_get_current_event(void) { return &event; }

bool etc_trigger(uint32_t value, uint32_t threshold) {
  /* Ignore if suppression is active */
  if (!ctimer_expired(&suppression_timer_new) ||
      !ctimer_expired(&suppression_timer_propagation))
    return false;

  /* Update event */
  event.seqn += (event.seqn == 0 ? 0 : 1);
  linkaddr_copy(&event.source, &linkaddr_node_addr);

  /* Prepare event message */
  struct event_msg_t event_msg;
  event_msg.seqn = event.seqn;
  linkaddr_copy(&event_msg.source, &event.source);

  /* Start to suppress new trigger(s) */
  ctimer_set(&suppression_timer_new, SUPPRESSION_TIMEOUT_NEW, NULL, NULL);

  /* Start to suppress new event message(s) */
  ctimer_set(&suppression_timer_propagation, SUPPRESSION_TIMEOUT_PROP, NULL,
             NULL);

  /* Schedule collect message dispatch */
  ctimer_set(&collect_timer, ETC_COLLECT_START_DELAY, collect_timer_cb, NULL);

  /* Send event message */
  return send_event_message(&event_msg);
}

int etc_command(const linkaddr_t *receiver, enum command_type_t command,
                uint32_t threshold) {
  /* Prepare and send command */
}

/* --- EVENT MESSAGE --- */
void event_msg_cb(const struct broadcast_hdr_t *header,
                  const linkaddr_t *sender) {
  struct event_msg_t event_msg;
  const enum node_role_t node_role = node_get_role();

  /* Check received event message validity */
  if (packetbuf_datalen() != sizeof(event_msg)) {
    LOG_ERROR("Received event message wrong size: %u byte",
              packetbuf_datalen());
    return;
  }

  /* Copy event message */
  packetbuf_copyto(&event_msg);

  /* Perform checks if node is sensor/actuator or forwarder */
  if (node_role == NODE_ROLE_SENSOR_ACTUATOR ||
      node_role == NODE_ROLE_FORWARDER) {
    /* Ignore if suppression is active */
    if (!ctimer_expired(&suppression_timer_new) ||
        !ctimer_expired(&suppression_timer_propagation))
      return;

    /* Ignore if already handling event */
    if (event_msg.seqn == event.seqn &&
        linkaddr_cmp(&event_msg.source, &event.source))
      return;
  }

  /* Update event */
  event.seqn = event_msg.seqn;
  linkaddr_copy(&event.source, &event_msg.source);

  LOG_INFO(
      "Received event message from %02x:%02x: "
      "{ seqn: %u, source: %02x:%02x}",
      sender->u8[0], sender->u8[1], event_msg.seqn, event_msg.source.u8[0],
      event_msg.source.u8[1]);

  /* If controller event callback */
  if (node_role == NODE_ROLE_CONTROLLER) {
    cb->event_cb(&event.source, event.seqn);
  }

  /* If node is sensor/actuator or forwarder */
  if (node_role == NODE_ROLE_SENSOR_ACTUATOR ||
      node_role == NODE_ROLE_FORWARDER) {
    /* Start to suppress new event message(s) */
    ctimer_set(&suppression_timer_propagation, SUPPRESSION_TIMEOUT_PROP, NULL,
               NULL);

    /* Schedule event message propagation */
    ctimer_set(&event_timer, ETC_EVENT_FORWARD_DELAY, event_timer_cb, NULL);

    /* Schedule collect message only if sensor/actuator */
    if (node_role == NODE_ROLE_SENSOR_ACTUATOR) {
      /* Schedule collect message dispatch */
      ctimer_set(&collect_timer, ETC_COLLECT_START_DELAY, collect_timer_cb,
                 NULL);
    }
  }
}

static void event_timer_cb(void *ignored) {
  /* Prepare event message */
  struct event_msg_t event_msg;
  event_msg.seqn = event.seqn;
  linkaddr_copy(&event_msg.source, &event.source);

  /* Send event message */
  send_event_message(&event_msg);
}

static bool send_event_message(const struct event_msg_t *event_msg) {
  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(event_msg, sizeof(struct event_msg_t));

  /* Send event message in broadcast */
  const bool ret = connection_broadcast_send(BROADCAST_MSG_TYPE_EVENT);
  if (!ret)
    LOG_ERROR("Error sending event message: %d", ret);
  else
    LOG_INFO("Sending event message: { seqn: %u, source: %02x:%02x }",
             event_msg->seqn, event_msg->source.u8[0], event_msg->source.u8[1]);

  return ret;
}

/* --- COLLECT MESSAGE--- */
static void collect_msg_cb(const struct unicast_hdr_t *header,
                           const linkaddr_t *sender) {
  /*TODO*/
  LOG_FATAL("HEYYYYYYYYYYYY BOYYYYYYYYYY from %02x:%02x", sender->u8[0],
            sender->u8[1]);
}

static void collect_timer_cb(void *ignored) {
  /* Prepare collect message */
  struct collect_msg_t collect_msg;
  collect_msg.event_seqn = event.seqn;
  linkaddr_copy(&collect_msg.event_source, &event.source);

  /* Send collect message */
  send_collect_message(&collect_msg, &connection_get_conn()->parent_node);
}

static bool send_collect_message(const struct collect_msg_t *collect_msg,
                                 const linkaddr_t *receiver) {
  if (!connection_is_connected()) {
    LOG_WARN(
        "Unable to send collect message because node is disconnected: "
        "{ seqn: %u, source: %02x:%02x }",
        collect_msg->event_seqn, collect_msg->event_source.u8[0],
        collect_msg->event_source.u8[1]);
    return false;
  }

  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(collect_msg, sizeof(struct collect_msg_t));

  /* Send collect message in unicast to receiver node */
  const bool ret = connection_unicast_send(UNICAST_MSG_TYPE_COLLECT, receiver);
  if (!ret)
    LOG_ERROR("Error sending collect message: %d", ret);
  else {
    sending_collect_msg = true;
    LOG_INFO(
        "Sending collect message to %02x:%02x: { seqn: %u, source: %02x:%02x }",
        receiver->u8[0], receiver->u8[1], collect_msg->event_seqn,
        collect_msg->event_source.u8[0], collect_msg->event_source.u8[1]);
  }

  return ret;
}

/* --- COMMAND MESSAGE --- */
static void command_msg_cb(const struct unicast_hdr_t *header,
                           const linkaddr_t *sender) {
  /* TODO */
}

/* --- CONNECTION --- */
/* --- Broadcast */
static void bc_recv(const struct broadcast_hdr_t *header,
                    const linkaddr_t *sender) {
  switch (header->type) {
    case BROADCAST_MSG_TYPE_EVENT: {
      event_msg_cb(header, sender);
      break;
    }
    default: {
      /* Ignore */
      break;
    }
  }
}

/* --- Unicast */
static void uc_recv(const struct unicast_hdr_t *header,
                    const linkaddr_t *sender) {
  switch (header->type) {
    case UNICAST_MSG_TYPE_COLLECT: {
      collect_msg_cb(header, sender);
      break;
    }
    case UNICAST_MSG_TYPE_COMMAND: {
      command_msg_cb(header, sender);
      break;
    }
    default: {
      /* Ignore */
      break;
    }
  }
}

static void uc_sent(int status, int num_tx) {
  if (status != MAC_TX_OK) {
    /* Something bad happended */

    /* Collect message */
    if (sending_collect_msg) {
      LOG_ERROR("Error sending collect message on tx %d due to %d", num_tx,
                status);
      sending_collect_msg = false;
      /* Retry */
      collect_timer_cb(NULL);
    }

    return;
  }

  /* Message sent */
  if (sending_collect_msg) {
    sending_collect_msg = false;
    LOG_DEBUG("Sent collect message");
  }
}
