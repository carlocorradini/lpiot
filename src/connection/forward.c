#include "forward.h"

/**
 * @brief Forwardings structure.
 */
static struct forward_t forwardings[NUM_SENSORS];

/**
 * @brief Reset forwardings structure.
 */
static void reset(void);

/**
 * @brief Print forwarindgs structure.
 */
static void print_forwardings(void);

/* --- --- */
void forward_init(void) { reset(); }

void forward_terminate(void) { reset(); }

struct forward_t* forward_find(const linkaddr_t* sensor) {
  size_t i;
  if (sensor == NULL) return NULL;

  for (i = 0; i < NUM_SENSORS; ++i) {
    if (linkaddr_cmp(sensor, &forwardings[i].sensor)) return &forwardings[i];
  }

  return NULL;
}

void forward_add(const linkaddr_t* sensor, const linkaddr_t* hop) {
  struct forward_t* f = forward_find(sensor);
  if (f == NULL || hop == NULL) return;

  /* Add */
  linkaddr_copy(&f->hop, hop);
  /* Print */
  print_forwardings();
}

void forward_remove(const linkaddr_t* sensor) {
  struct forward_t* f = forward_find(sensor);
  if (f == NULL) return;

  LOG_DEBUG("Removing hop %02x:%02x for sensor %02x:%02x", f->hop.u8[0],
            f->hop.u8[1], sensor->u8[0], sensor->u8[1]);

  /* Remove */
  linkaddr_copy(&f->hop, &linkaddr_null);
  /* Print */
  print_forwardings();
}

bool forward_hop_available(const linkaddr_t* sensor) {
  struct forward_t* f = forward_find(sensor);
  return f != NULL && !linkaddr_cmp(&f->hop, &linkaddr_null);
}

/* --- RESET --- */
static void reset(void) {
  size_t i;

  for (i = 0; i < NUM_SENSORS; ++i) {
    linkaddr_copy(&forwardings[i].sensor, &SENSORS[i]);
    linkaddr_copy(&forwardings[i].hop, &linkaddr_null);
  }
}

static void print_forwardings(void) {
  if (!logger_is_enabled(LOG_LEVEL_DEBUG)) return;

  size_t i;
  const struct forward_t* f;

  logger_set_newline(false);
  LOG_DEBUG("Forwardings: ");
  printf("[ ");
  for (i = 0; i < NUM_SENSORS; ++i) {
    f = &forwardings[i];
    printf("%u{ node: %02x:%02x, hop: %02x:%02x } ", i, f->sensor.u8[0],
           f->sensor.u8[1], f->hop.u8[0], f->hop.u8[1]);
  }
  printf("]\n");
  logger_set_newline(true);
}
