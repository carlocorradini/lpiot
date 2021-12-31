#include "etc.h"

#include "config/config.h"
#include "net/packetbuf.h"

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

/**
 * @brief  Event message.
 */
struct event_msg_t {
  /**
   * @brief Address of the sensor that generated the event.
   */
  linkaddr_t event_source;
  /**
   * @brief Event sequence number.
   */
  uint16_t event_seqn;
} __attribute__((packed));

/* FIXME Mumble on header */
/**
 * @brief Header structure for data packets.
 */
struct collect_msg_t {
  /**
   * @brief Address of the sensor that generated the event.
   */
  linkaddr_t event_source;
  /**
   * @brief Event sequence number.
   */
  uint16_t event_seqn;
} __attribute__((packed));

/* FIXME Mumble on header */
/**
 * @brief Header structure for command packets.
 */
struct command_msg_t {
  /**
   * @brief Address of the sensor that generated the event.
   */
  linkaddr_t event_source;
  /**
   * @brief Event sequence number.
   */
  uint16_t event_seqn;
} __attribute__((packed));

/**
 * @brief Event timer callback.
 *
 * @param ignored
 */
static void event_timer_cb(void *ignored);

/**
 * @brief Collect timer callback.
 *
 * @param ignored
 */
static void collect_timer_cb(void *ignored);

/**
 * @brief Send event message.
 *
 * @param event_msg Event message to send.
 * @return
 */
/*TODO RETURN*/
static int send_event_message(const struct event_msg_t *event_msg);

/**
 * @brief Send collect message to receiver node.
 *
 * @param collect_msg Collect message to send.
 * @param receiver Receiver node address.
 * @return
 */
/* TODO RETURN*/
static int send_collect_message(const struct collect_msg_t *collect_msg,
                                const linkaddr_t *receiver);

void etc_open(uint16_t channel, const struct etc_callbacks_t *callbacks) {
  cb = callbacks;
  event.seqn = 0;
  linkaddr_copy(&event.source, &linkaddr_null);

  /* Open connection */
  connection_open(channel);
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

int etc_trigger(uint32_t value, uint32_t threshold) {
  /* Ignore if suppression is active */
  /* TODO return no value! */
  if (!ctimer_expired(&suppression_timer_new)) return;

  /* Current event is me */
  event.seqn += 1;
  linkaddr_copy(&event.source, &linkaddr_node_addr);

  /* Prepare event message */
  struct event_msg_t event_msg;
  event_msg.event_seqn = event.seqn;
  linkaddr_copy(&event_msg.event_source, &event.source);

  /* Start to suppress new trigger(s) */
  ctimer_set(&suppression_timer_new, SUPPRESSION_TIMEOUT_NEW, NULL, NULL);

  /* Send event message in broadcast */
  return send_event_message(&event_msg);
}

int etc_command(const linkaddr_t *receiver, enum command_type_t command,
                uint32_t threshold) {
  /* Prepare and send command */
}

void etc_event_cb(const struct broadcast_hdr_t *header,
                  const linkaddr_t *sender) {
  struct event_msg_t event_msg;

  /* Ignore if suppression is active */
  if (!ctimer_expired(&suppression_timer_new) ||
      !ctimer_expired(&suppression_timer_propagation))
    return;
  /* Check received event message validity */
  if (packetbuf_datalen() != sizeof(event_msg)) {
    LOG_ERROR("Received event message wrong size: %u byte",
              packetbuf_datalen());
    return;
  }

  /* Copy event message */
  packetbuf_copyto(&event_msg);

  /* Ignore event if made by me */
  if (event_msg.event_seqn == event.seqn &&
      linkaddr_cmp(&event_msg.event_source, &event.source))
    return;

  LOG_INFO(
      "Received event message from %02x:%02x: "
      "{ seqn: %u, source: %02x:%02x}",
      sender->u8[0], sender->u8[1], event_msg.event_seqn,
      event_msg.event_source.u8[0], event_msg.event_source.u8[1]);

  /* Update current event */
  event.seqn = event_msg.event_seqn;
  linkaddr_copy(&event.source, &event_msg.event_source);

  /* Start to suppress new event message(s) */
  ctimer_set(&suppression_timer_propagation, SUPPRESSION_TIMEOUT_PROP, NULL,
             NULL);

  /* Schedule event message propagation */
  ctimer_set(&event_timer, ETC_EVENT_FORWARD_DELAY, event_timer_cb, NULL);

  /* Schedule collect only if sensor/actuator */
  if (node_get_role() == NODE_ROLE_SENSOR_ACTUATOR) {
    /* Schedule collect message dispatch */
    ctimer_set(&collect_timer, ETC_COLLECT_START_DELAY, collect_timer_cb, NULL);
  }
}

void etc_collect_cb(const struct unicast_hdr_t *header,
                    const linkaddr_t *sender) {
  /*TODO*/
  LOG_FATAL("HEYYYYYYYYYYYY BOYYYYYYYYYY");
}

static int send_event_message(const struct event_msg_t *event_msg) {
  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(event_msg, sizeof(struct event_msg_t));

  /* Send event message in broadcast */
  const int ret = connection_broadcast_send(BROADCAST_MSG_TYPE_EVENT);
  if (!ret)
    LOG_ERROR("Error sending event message: %d", ret);
  else
    LOG_INFO("Sending event message: { seqn: %u, source: %02x:%02x }",
             event_msg->event_seqn, event_msg->event_source.u8[0],
             event_msg->event_source.u8[1]);

  return ret;
}

static int send_collect_message(const struct collect_msg_t *collect_msg,
                                const linkaddr_t *receiver) {
  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(collect_msg, sizeof(struct collect_msg_t));

  /* Send collect message in unicast to receiver node */
  const int ret = connection_unicast_send(UNICAST_MSG_TYPE_COLLECT, receiver);
  if (!ret)
    LOG_ERROR("Error sending collect message: %d", ret);
  else
    LOG_INFO(
        "Sending collect message to %02x:%02x: { seqn: %u, source: %02x:%02x }",
        receiver->u8[0], receiver->u8[1], collect_msg->event_seqn,
        collect_msg->event_source.u8[0], collect_msg->event_source.u8[1]);

  return ret;
}

static void event_timer_cb(void *ignored) {
  /* Prepare event message */
  struct event_msg_t event_msg;
  event_msg.event_seqn = event.seqn;
  linkaddr_copy(&event_msg.event_source, &event.source);

  /* Send event message */
  send_event_message(&event_msg);
}

static void collect_timer_cb(void *ignored) {
  /* Prepare collect message */
  struct collect_msg_t collect_msg;
  collect_msg.event_seqn = event.seqn;
  linkaddr_copy(&collect_msg.event_source, &event.source);

  /* Send collect message */
  send_collect_message(&collect_msg, &best_conn->parent_node);
}
