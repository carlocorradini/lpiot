#include "etc.h"

#include "config/config.h"
#include "connection/connection.h"
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
  /* Prepare event message */
  struct event_msg_t event_msg;
  event_msg.event_seqn = event.seqn;
  linkaddr_copy(&event_msg.event_source, &linkaddr_node_addr);

  /* Prepare packetbuf */
  packetbuf_clear();
  packetbuf_copyfrom(&event_msg, sizeof(struct event_msg_t));

  /* Send event message in broadcast */
  return connection_broadcast_send(BROADCAST_MSG_TYPE_BEACON);
}

int etc_command(const linkaddr_t *receiver, enum command_type_t command,
                uint32_t threshold) {
  /* Prepare and send command */
}
