#include "config.h"

/* --- CONTROLLER --- */
#ifndef CONTIKI_TARGET_SKY
const linkaddr_t CONTROLLER = {{0xF7, 0x9C}}; /* Firefly node 1 */
#else
const linkaddr_t CONTROLLER = {{0x01, 0x00}}; /* Sky node 1 */
#endif

/* --- SENSOR --- */
#ifndef CONTIKI_TARGET_SKY
const linkaddr_t SENSORS[NUM_SENSORS] = {
    {{0xF3, 0x84}}, /* Firefly node 3 */
    {{0xF2, 0x33}}, /* Firefly node 12 */
    {{0xf3, 0x8b}}, /* Firefly node 18 */
    {{0xF3, 0x88}}, /* Firefly node 22*/
    {{0xF7, 0xE1}}  /* Firefly node 30 */
};
#else
const linkaddr_t SENSORS[NUM_SENSORS] = {{{0x02, 0x00}},
                                         {{0x03, 0x00}},
                                         {{0x04, 0x00}},
                                         {{0x05, 0x00}},
                                         {{0x06, 0x00}}};
#endif