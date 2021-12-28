#include <core/dev/button-sensor.h>
#include <core/dev/leds.h>
#include <core/net/netstack.h>
#include <stdbool.h>
#include <stdio.h>

#include "configs/config.h"
#include "nodes/controller/controller.h"
#include "nodes/forwarder/forwarder.h"
#include "nodes/sensor/sensor.h"
#include "tools/simple-energest.h"

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

  printf("[APP]: I am node %02x:%02x\n", linkaddr_node_addr.u8[0],
         linkaddr_node_addr.u8[1]);

  while (true) {
    if (linkaddr_cmp(&CONTROLLER, &linkaddr_node_addr)) {
      /* --- Controller */
      controller_init(&etc_conn);
      printf("[APP]: Controller started\n");
    } else {
      bool is_sensor = false;
      uint i;
      for (i = 0; i < NUM_SENSORS; ++i) {
        if (linkaddr_cmp(&SENSORS[i], &linkaddr_node_addr)) {
          /* --- Sensor/Actuator */
          is_sensor = true;
          sensor_init(&etc_conn, i);
          printf("[APP]: Sensor/Actuator started\n");
          break;
        }
      }

      if (!is_sensor) {
        /* --- Forwarder */
        forwarder_init(&etc_conn);
        printf("[APP]: Forwarder started\n");
      }
    }

    /* Wait button press | Node failure simulation */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    printf("[APP]: Simulating node failure\n");
    etc_close(&etc_conn);
    NETSTACK_MAC.off(false);
    leds_on(LEDS_RED);

    /* Wait button press | Node failure recovery */
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event);
    printf("[APP]: Simulating node recovery\n");
    NETSTACK_MAC.on();
    leds_off(LEDS_RED);
  }

  PROCESS_END();
}
