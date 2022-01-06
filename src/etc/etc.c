#include "etc.h"

#include <net/mac/mac.h>
#include <net/packetbuf.h>

#include "config/config.h"
#include "connection/forward.h"

/**
 * @brief Event message.
 */
struct event_msg_t {
  /* Event sequence number. */
  uint16_t seqn;
  /* Address of the sensor that generated the event. */
  linkaddr_t source;
} __attribute__((packed));

/**
 * @brief Collect message.
 */
struct collect_msg_t {
  /* Event sequence number. */
  uint16_t event_seqn;
  /* Address of the sensor that generated the event. */
  linkaddr_t event_source;
  /* Address of sender sensor node. */
  linkaddr_t sender;
  /* Node value. */
  uint32_t value;
  /* Node threshold. */
  uint32_t threshold;
} __attribute__((packed));

/**
 * @brief Command message.
 */
struct command_msg_t {
  /* Event sequence number. */
  uint16_t event_seqn;
  /* Address of the sensor that generated the event. */
  linkaddr_t event_source;
  /* Adress of receiver actuator node. */
  linkaddr_t receiver;
  /* Command type. */
  enum command_type_t command;
  /* New threshold. */
  uint32_t threshold;
} __attribute__((packed));

/* Sensor value. */
static uint32_t sensor_value;

/* Sensor threshold. */
static uint32_t sensor_threshold;

/* Sensor event seqn */
static uint32_t sensor_event_seqn;

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
 * @brief Timer to wait before stop event(s) propagation timer after a command
 * message is received.
 */
static struct ctimer suppression_timer_propagation_end;

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

/**
 * @brief Suppression timer propagation end callback.
 *
 * @param ignored
 */
static void suppression_timer_propagation_end_cb(void *ignored);

/**
 * @brief Send command message to receiver node.
 *
 * @param command_msg Command message to send.
 * @param receiver Receiver node address.
 * @return true Command message sent.
 * @return false Command message not sent due to an error.
 */
static bool send_command_message(const struct command_msg_t *command_msg,
                                 const linkaddr_t *receiver);

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
 * @param status Status.
 */
static void uc_sent(bool status);

/**
 * @brief Connection callbacks.
 */
static struct connection_callbacks_t conn_cb = {
    .bc = {.recv = bc_recv, .sent = NULL},
    .uc = {.recv = uc_recv, .sent = uc_sent}};

/* --- --- */
void etc_open(uint16_t channel, const struct etc_callbacks_t *callbacks) {
  cb = callbacks;

  /* Event */
  event.seqn = 0;
  linkaddr_copy(&event.source, &linkaddr_null);

  /* Sensor */
  sensor_event_seqn = 0;
  sensor_value = 0;
  sensor_threshold = 0;

  /* Open connection */
  connection_open(channel, &conn_cb);
}

void etc_close(void) {
  cb = NULL;

  /* Event */
  sensor_event_seqn = 0;
  event.seqn = 0;
  linkaddr_copy(&event.source, &linkaddr_null);

  /* Sensor */
  sensor_value = 0;
  sensor_threshold = 0;

  /* Timers */
  ctimer_stop(&suppression_timer_new);
  ctimer_stop(&suppression_timer_propagation);
  ctimer_stop(&suppression_timer_propagation_end);
  ctimer_stop(&event_timer);
  ctimer_stop(&collect_timer);

  /* Close connection */
  connection_close();
}

const struct etc_event_t *etc_get_current_event(void) { return &event; }

void etc_update(uint32_t value, uint32_t threshold) {
  /* Update sensor data */
  sensor_value = value;
  sensor_threshold = threshold;
}

bool etc_trigger(uint32_t value, uint32_t threshold) {
  /* Ignore if suppression is active */
  if (!ctimer_expired(&suppression_timer_new) ||
      !ctimer_expired(&suppression_timer_propagation))
    return false;

  /* Update event */
  sensor_event_seqn += 1;
  event.seqn = sensor_event_seqn;
  linkaddr_copy(&event.source, &linkaddr_node_addr);

  /* Start to suppress new event(s) */
  ctimer_set(&suppression_timer_new, ETC_SUPPRESSION_EVENT_NEW, NULL, NULL);

  /* Start to suppress event propagation */
  ctimer_set(&suppression_timer_propagation, ETC_SUPPRESSION_EVENT_PROPAGATION,
             NULL, NULL);

  /* Schedule collect message dispatch */
  ctimer_set(&collect_timer, ETC_COLLECT_START_DELAY, collect_timer_cb, NULL);

  /* Trigger event timer manually */
  event_timer_cb(NULL);

  return true;
}

bool etc_command(const linkaddr_t *receiver, enum command_type_t command,
                 uint32_t threshold) {
  struct command_msg_t command_msg;
  struct forward_t *forward = forward_find(receiver);

  /* Prepare command message */
  command_msg.event_seqn = event.seqn;
  linkaddr_copy(&command_msg.event_source, &event.source);
  linkaddr_copy(&command_msg.receiver, receiver);
  command_msg.command = command;
  command_msg.threshold = threshold;

  /* Check if forwarding rule exists */
  if (forward == NULL || linkaddr_cmp(&forward->hops[0], &linkaddr_null)) {
    LOG_ERROR(
        "Unable to forward command message because no forwarding rule has "
        "been found");
    return false;
  }

  /* Send */
  return send_command_message(&command_msg, &forward->hops[0]);
}

/* --- EVENT MESSAGE --- */
void event_msg_cb(const struct broadcast_hdr_t *header,
                  const linkaddr_t *sender) {
  struct event_msg_t event_msg;
  const enum node_role_t node_role = node_get_role();

  /* Ignore if suppression is active */
  if (!ctimer_expired(&suppression_timer_new) ||
      !ctimer_expired(&suppression_timer_propagation)) {
    LOG_WARN("Event message propagation is suppressed");
    return;
  }

  /* Check received event message validity */
  if (packetbuf_datalen() != sizeof(event_msg)) {
    LOG_ERROR("Received event message wrong size: %u byte",
              packetbuf_datalen());
    return;
  }

  /* Copy event message */
  packetbuf_copyto(&event_msg);

  LOG_INFO(
      "Received event message from %02x:%02x: "
      "{ seqn: %u, source: %02x:%02x }",
      sender->u8[0], sender->u8[1], event_msg.seqn, event_msg.source.u8[0],
      event_msg.source.u8[1]);

  /* Ignore if already handling event */
  if (event_msg.seqn == event.seqn &&
      linkaddr_cmp(&event_msg.source, &event.source)) {
    LOG_WARN("Already handling event: { seqn: %u, source: %02x:%02x }",
             event_msg.seqn, event_msg.source.u8[0], event_msg.source.u8[1]);
    return;
  }

  /* Update event */
  event.seqn = event_msg.seqn;
  linkaddr_copy(&event.source, &event_msg.source);

  /* If controller forward to event callback */
  if (node_role == NODE_ROLE_CONTROLLER) {
    cb->event_cb(event.seqn, &event.source);
  }

  /* Start to suppress new event message(s) */
  ctimer_set(&suppression_timer_propagation, ETC_SUPPRESSION_EVENT_PROPAGATION,
             NULL, NULL);

  /* Schedule event message propagation */
  ctimer_set(&event_timer, ETC_EVENT_FORWARD_DELAY, event_timer_cb, NULL);

  /* Schedule collect message only if sensor/actuator */
  if (node_role == NODE_ROLE_SENSOR_ACTUATOR) {
    /* Schedule collect message dispatch */
    ctimer_set(&collect_timer, ETC_COLLECT_START_DELAY, collect_timer_cb, NULL);
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

/* --- COLLECT MESSAGE --- */
static void collect_msg_cb(const struct unicast_hdr_t *header,
                           const linkaddr_t *sender) {
  struct collect_msg_t collect_msg;

  /* Check received collect message validity */
  if (packetbuf_datalen() != sizeof(collect_msg)) {
    LOG_ERROR("Received collect message wrong size: %u byte",
              packetbuf_datalen());
    return;
  }

  /* Copy collect message */
  packetbuf_copyto(&collect_msg);

  LOG_INFO(
      "Received collect message from %02x:%02x: "
      "{ event_seqn: %u, event_source: %02x:%02x, "
      "sender: %02x:%02x, value: %lu, threshold: %lu}",
      sender->u8[0], sender->u8[1], collect_msg.event_seqn,
      collect_msg.event_source.u8[0], collect_msg.event_source.u8[1],
      collect_msg.sender.u8[0], collect_msg.sender.u8[1], collect_msg.value,
      collect_msg.threshold);

  /* Update forwarding rule */
  forward_add(&collect_msg.sender, sender);

  /* Forward based on node role */
  switch (node_get_role()) {
    case NODE_ROLE_SENSOR_ACTUATOR:
    case NODE_ROLE_FORWARDER: {
      /* Check connection */
      if (!connection_is_connected()) {
        LOG_WARN(
            "Unable to forward collect message because the node is "
            "disconnected");
        return;
      }

      /* Forward collect message to parent node */
      /* FIXME In forward non voglio riscrivere packetbuf */
      send_collect_message(&collect_msg, &connection_get_conn()->parent_node);
      break;
    }
    case NODE_ROLE_CONTROLLER: {
      /* Forward to collect callback */
      cb->collect_cb(collect_msg.event_seqn, &collect_msg.event_source,
                     &collect_msg.sender, collect_msg.value,
                     collect_msg.threshold);
      break;
    }
    default:
      /* Ignore */
      break;
  }
}

static void collect_timer_cb(void *ignored) {
  /* Prepare collect message */
  struct collect_msg_t collect_msg;
  collect_msg.event_seqn = event.seqn;
  linkaddr_copy(&collect_msg.event_source, &event.source);
  linkaddr_copy(&collect_msg.sender, &linkaddr_node_addr);
  collect_msg.value = sensor_value;
  collect_msg.threshold = sensor_threshold;

  /* Send collect message */
  send_collect_message(&collect_msg, &connection_get_conn()->parent_node);
}

static bool send_collect_message(const struct collect_msg_t *collect_msg,
                                 const linkaddr_t *receiver) {
  /* Check connection */
  if (!connection_is_connected()) {
    LOG_WARN(
        "Unable to forward collect message because the node is "
        "disconnected");
    return false;
  }

  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(collect_msg, sizeof(struct collect_msg_t));

  /* Send collect message in unicast to receiver node */
  const bool ret =
      connection_unicast_send(UNICAST_MSG_TYPE_COLLECT, receiver, NULL);
  if (!ret)
    LOG_ERROR(
        "Error sending collect message to %02x:%02x: "
        "{ event_seqn: %u, event_source: %02x:%02x, "
        "sender: %02x:%02x, value: %lu, threshold: %lu}",
        receiver->u8[0], receiver->u8[1], collect_msg->event_seqn,
        collect_msg->event_source.u8[0], collect_msg->event_source.u8[1],
        collect_msg->sender.u8[0], collect_msg->sender.u8[1],
        collect_msg->value, collect_msg->threshold);
  else {
    LOG_INFO(
        "Sending collect message to %02x:%02x: "
        "{ event_seqn: %u, event_source: %02x:%02x, "
        "sender: %02x:%02x, value: %lu, threshold: %lu}",
        receiver->u8[0], receiver->u8[1], collect_msg->event_seqn,
        collect_msg->event_source.u8[0], collect_msg->event_source.u8[1],
        collect_msg->sender.u8[0], collect_msg->sender.u8[1],
        collect_msg->value, collect_msg->threshold);
  }

  return ret;
}

/* --- COMMAND MESSAGE --- */
static void command_msg_cb(const struct unicast_hdr_t *header,
                           const linkaddr_t *sender) {
  struct command_msg_t command_msg;

  /* Check received command message validity */
  if (packetbuf_datalen() != sizeof(command_msg)) {
    LOG_ERROR("Received command message wrong size: %u byte",
              packetbuf_datalen());
    return;
  }

  /* Copy command message */
  packetbuf_copyto(&command_msg);

  LOG_INFO(
      "Received command message from %02x:%02x: "
      "{ receiver: %02x:%02x, command: %d, threshold: %lu, event_seqn: %u, "
      "event_source: %02x:%02x }",
      sender->u8[0], sender->u8[1], command_msg.receiver.u8[0],
      command_msg.receiver.u8[1], command_msg.command, command_msg.threshold,
      command_msg.event_seqn, command_msg.event_source.u8[0],
      command_msg.event_source.u8[1]);

  /* Check receiver address */
  if (!linkaddr_cmp(&command_msg.receiver, &linkaddr_node_addr)) {
    /* Forward */
    const struct forward_t *forward = forward_find(&command_msg.receiver);

    /* Check if forwarding rule exists */
    if (forward == NULL || linkaddr_cmp(&forward->hops[0], &linkaddr_null)) {
      LOG_ERROR(
          "Unable to forward command message because no forwarding rule has "
          "been found");
      return;
    }

    /* Forward to next hop node */
    send_command_message(&command_msg, &forward->hops[0]);
    return;
  }

  /* Me */
  /* Forward to command callback */
  cb->command_cb(command_msg.event_seqn, &command_msg.event_source,
                 command_msg.command, command_msg.threshold);

  /* Schedule stop event propagation suppression */
  ctimer_set(&suppression_timer_propagation_end,
             ETC_SUPPRESSION_EVENT_PROPAGATION_END,
             suppression_timer_propagation_end_cb, NULL);
}

static void suppression_timer_propagation_end_cb(void *ignored) {
  /* Stop event propagation suppression */
  ctimer_stop(&suppression_timer_propagation);
}

static bool send_command_message(const struct command_msg_t *command_msg,
                                 const linkaddr_t *receiver) {
  /* Check connection */
  if (node_get_role() != NODE_ROLE_CONTROLLER && !connection_is_connected()) {
    LOG_WARN(
        "Unable to send command message because the node is "
        "disconnected");
    return false;
  }

  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(command_msg, sizeof(struct command_msg_t));

  /* Send command message in unicast to receiver node */
  const bool ret = connection_unicast_send(UNICAST_MSG_TYPE_COMMAND, receiver,
                                           &command_msg->receiver);
  if (!ret)
    LOG_ERROR(
        "Error sending command message to %02x:%02x: "
        "{ receiver: %02x:%02x, command: %d, threshold: %lu, event_seqn: %u, "
        "event_source: %02x:%02x }",
        receiver->u8[0], receiver->u8[1], command_msg->receiver.u8[0],
        command_msg->receiver.u8[1], command_msg->command,
        command_msg->threshold, command_msg->event_seqn,
        command_msg->event_source.u8[0], command_msg->event_source.u8[1]);
  else {
    LOG_INFO(
        "Sending command message to %02x:%02x: "
        "{ receiver: %02x:%02x, command: %d, threshold: %lu, event_seqn: %u, "
        "event_source: %02x:%02x }",
        receiver->u8[0], receiver->u8[1], command_msg->receiver.u8[0],
        command_msg->receiver.u8[1], command_msg->command,
        command_msg->threshold, command_msg->event_seqn,
        command_msg->event_source.u8[0], command_msg->event_source.u8[1]);
  }

  return ret;
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

static void uc_sent(bool status) {
  if (!status) {
    /* ERROR */
    LOG_ERROR("Unicast message not sent");
  } else {
    /* SUCCESS */
    LOG_INFO("Unicast message sent");
  }
}
