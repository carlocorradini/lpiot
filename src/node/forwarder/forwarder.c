#include "forwarder.h"

#include "config/config.h"

/**
 * @brief Connection.
 */
static struct etc_conn_t *etc_conn = NULL;

/**
 * @brief Callbacks.
 */
static struct etc_callbacks_t cb = {
    .receive_cb = NULL, .event_cb = NULL, .command_cb = NULL};

void forwarder_init(struct etc_conn_t *conn) {
  etc_conn = conn;

  /* Open connection */
  etc_open(etc_conn, ETC_FIRST_CHANNEL, NODE_ROLE_FORWARDER, &cb, SENSORS,
           NUM_SENSORS);
}