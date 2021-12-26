#include "etc.h"

#include <stdbool.h>
#include <stdio.h>

#include "contiki.h"
#include "core/net/linkaddr.h"
#include "leds.h"
#include "lib/random.h"
#include "net/netstack.h"
#include "net/rime/rime.h"
/*---------------------------------------------------------------------------*/
/* A simple debug system to enable/disable some printfs */
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
/* Topology information (parents, metrics...) */
/* ... */
/*---------------------------------------------------------------------------*/
/* Forwarders (routes to sensors/actuators) */
/* ... */
/*---------------------------------------------------------------------------*/
/* Declarations for the callbacks of dedicated connection objects */
/* ... */
/*---------------------------------------------------------------------------*/
/* Rime Callback structures */
/* ... */
/*---------------------------------------------------------------------------*/
/*                           Application Interface                           */
/*---------------------------------------------------------------------------*/
/* Create connection(s) and start the protocol */
bool etc_open(struct etc_conn_t *conn, uint16_t channels, node_role_t node_role,
              const struct etc_callbacks_t *callbacks, linkaddr_t *sensors,
              uint8_t num_sensors) {
  /* Initialize the connector structure */

  /* Open the underlying Rime primitives */

  /* Initialize sensors forwarding structure */

  /* Tree construction (periodic) */
}
/*---------------------------------------------------------------------------*/
/* Turn off the protocol */
void etc_close(struct etc_conn_t *conn) {
  /* Turn off connections to ignore any incoming packet
   * and stop transmitting */
}
/*---------------------------------------------------------------------------*/
/* Used by the app to share the most recent sensed value;
 * ONLY USED BY SENSORS */
void etc_update(uint32_t value, uint32_t threshold) {
  /* Update local value and threshold, to be sent in case of event */
}
/*---------------------------------------------------------------------------*/
/* Start event dissemination (unless events are being suppressed to avoid
 * contention).
 * Returns 0 if new events are currently being suppressed.
 * ONLY USED BY SENSORS */
int etc_trigger(struct etc_conn_t *conn, uint32_t value, uint32_t threshold) {
  /* Prepare event message */

  /* Suppress other events for a given time window */

  /* Send event */
}
/*---------------------------------------------------------------------------*/
/* Called by the controller to send commands to a given destination.
 * ONLY USED BY CONTROLLER */
int etc_command(struct etc_conn_t *conn, const linkaddr_t *dest,
                command_type_t command, uint32_t threshold) {
  /* Prepare and send command */
}
/*---------------------------------------------------------------------------*/
/*                              Beacon Handling                              */
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/
/*                               Event Handling                              */
/*---------------------------------------------------------------------------*/
/* Event message structure, combining event source (address of the sensor
 * generating the event) and a sequence number. */
struct event_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
} __attribute__((packed));
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/
/*                               Data Handling                               */
/*---------------------------------------------------------------------------*/
/* Header structure for data packets */
struct collect_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
  /* ... */
} __attribute__((packed));
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/
/*                               Command Handling                            */
/*---------------------------------------------------------------------------*/
/* Header structure for data packets */
struct command_msg_t {
  linkaddr_t event_source;
  uint16_t event_seqn;
  /* ... */
} __attribute__((packed));
/*---------------------------------------------------------------------------*/
/* ... */
/*---------------------------------------------------------------------------*/