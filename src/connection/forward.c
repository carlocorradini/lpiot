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
 * @brief Shift left hops of a sensor.
 *
 * @param f Forward entry.
 * @param from From index to shift.
 */
static void shift_left(struct forward_t* f, size_t from);

/**
 * @brief Shift right hops of sensor.
 *
 * @param f Forward entry.
 */
static void shift_right(struct forward_t* f);

/**
 * @brief Print forwarindgs structure.
 */
static void print_forwardings(void);

/* --- --- */
void forward_init(void) { reset(); }

void forward_terminate(void) { reset(); }

struct forward_t* forward_find(const linkaddr_t* sensor) {
  size_t i;

  for (i = 0; i < NUM_SENSORS; ++i) {
    if (linkaddr_cmp(sensor, &forwardings[i].sensor)) return &forwardings[i];
  }

  return NULL;
}

void forward_add(const linkaddr_t* sensor, const linkaddr_t* next_hop) {
  struct forward_t* f = forward_find(sensor);
  size_t i;

  if (f == NULL) return;

  /* Remove duplicates */
  for (i = 0; i < CONNECTION_FORWARD_MAX_SIZE; ++i) {
    if (linkaddr_cmp(&f->hops[i], next_hop)) {
      shift_left(f, i);
    }
  }

  /* Add */
  shift_right(f);
  linkaddr_copy(&f->hops[0], next_hop);

  /* Print */
  print_forwardings();
}

void forward_remove(const linkaddr_t* sensor) {
  struct forward_t* f = forward_find(sensor);

  if (f == NULL) return;

  LOG_WARN("Removing hop %02x:%02x for sensor %02x:%02x", f->hops[0].u8[0],
           f->hops[0].u8[1], sensor->u8[0], sensor->u8[1]);

  /* Remove */
  shift_left(f, 0);

  /* Print */
  print_forwardings();
}

void forward_remove_hop(const linkaddr_t* sensor,
                        const linkaddr_t* hop_address) {
  struct forward_t* f = forward_find(sensor);
  size_t i;

  if (f == NULL) return;

  for (i = 0; i < CONNECTION_FORWARD_MAX_SIZE; ++i) {
    if (linkaddr_cmp(hop_address, &f->hops[i])) break;
  }
  if (i >= CONNECTION_FORWARD_MAX_SIZE) return;

  LOG_WARN(
      "Removing hop at %d for sensor %02x:%02x: "
      "%02x:%02x",
      i, f->sensor.u8[0], f->sensor.u8[1], f->hops[i].u8[0], f->hops[i].u8[1]);

  /* Remove */
  shift_left(f, i);

  /* Print */
  print_forwardings();
}

size_t forward_hops_length(const linkaddr_t* sensor) {
  struct forward_t* f = forward_find(sensor);
  size_t i;
  size_t length = 0;

  if (f == NULL) return length;

  for (i = 0; i < CONNECTION_FORWARD_MAX_SIZE; ++i) {
    if (linkaddr_cmp(&f->hops[i], &linkaddr_null)) break;
    length += 1;
  }

  return length;
}

/* --- RESET --- */
static void reset(void) {
  size_t i;
  size_t j;

  for (i = 0; i < NUM_SENSORS; ++i) {
    linkaddr_copy(&forwardings[i].sensor, &SENSORS[i]);
    for (j = 0; j < CONNECTION_FORWARD_MAX_SIZE; ++j) {
      linkaddr_copy(&forwardings[i].hops[j], &linkaddr_null);
    }
  }
}

static void shift_right(struct forward_t* f) {
  size_t i;

  if (f == NULL) return;

  for (i = CONNECTION_FORWARD_MAX_SIZE - 1; i > 0; --i) {
    linkaddr_copy(&f->hops[i], &f->hops[i - 1]);
  }

  linkaddr_copy(&f->hops[0], &linkaddr_null);
}

static void shift_left(struct forward_t* f, size_t from) {
  size_t i;

  if (f == NULL) return;

  for (i = from; i < CONNECTION_FORWARD_MAX_SIZE - 1; ++i) {
    linkaddr_copy(&f->hops[i], &f->hops[i + 1]);
  }

  linkaddr_copy(&f->hops[i], &linkaddr_null);
}

static void print_forwardings(void) {
  if (!logger_is_enabled(LOG_LEVEL_DEBUG)) return;

  size_t i;
  size_t j;
  const struct forward_t* f;

  logger_set_newline(false);
  LOG_DEBUG("Forwardings: ");
  printf("[ ");
  for (i = 0; i < NUM_SENSORS; ++i) {
    f = &forwardings[i];
    printf("%u{ node: %02x:%02x, hops: [ ", i, f->sensor.u8[0],
           f->sensor.u8[1]);
    for (j = 0; j < CONNECTION_FORWARD_MAX_SIZE; ++j) {
      printf("%02x:%02x ", f->hops[j].u8[0], f->hops[j].u8[1]);
    }
    printf("] } ");
  }
  printf("]\n");
  logger_set_newline(true);
}
