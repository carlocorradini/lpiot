#include <dev/button-sensor.h>
#include <dev/leds.h>
#include <net/netstack.h>

#include "config/config.h"
#include "logger/logger.h"
#include "node/controller/controller.h"
#include "node/forwarder/forwarder.h"
#include "node/node.h"
#include "node/sensor/sensor.h"
#include "tool/simple-energest.h"

/**
 * @brief Connection.
 */
static struct etc_conn_t etc_conn;

/* --- APPLICATION --- */
PROCESS(app_process, "App process");
AUTOSTART_PROCESSES(&app_process);
PROCESS_THREAD(app_process, ev, data) {
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);

  /* Start energest */
  simple_energest_start();

  LOG_INFO("I am %s %02x:%02x", node_get_role_name(), linkaddr_node_addr.u8[0],
           linkaddr_node_addr.u8[1]);

  while (true) {
    const node_role_t node_role = node_get_role();

    if (node_role == NODE_ROLE_CONTROLLER) {
      controller_init(&etc_conn);
    } else if (node_role == NODE_ROLE_SENSOR_ACTUATOR) {
      size_t i;
      for (i = 0; i < NUM_SENSORS; ++i) {
        if (linkaddr_cmp(&SENSORS[i], &linkaddr_node_addr)) {
          sensor_init(&etc_conn, i);
          break;
        }
      }
    } else if (node_role == NODE_ROLE_FORWARDER) {
      forwarder_init(&etc_conn);
    } else {
      LOG_FATAL("Unknown role. Terminating...");
      break;
    }

    LOG_INFO("Node started");

    /* Wait button press | Node failure simulation */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    LOG_WARN("Simulating node failure");
    etc_close(&etc_conn);
    NETSTACK_MAC.off(false);
    leds_on(LEDS_RED);

    /* Wait button press | Node failure recovery */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    LOG_WARN("Simulating node recovery");
    NETSTACK_MAC.on();
    leds_off(LEDS_RED);
  }

  PROCESS_END();
}
