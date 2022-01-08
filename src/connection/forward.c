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

void forward_add(const linkaddr_t* sensor, const linkaddr_t* hop_address,
                 uint8_t hop_distance) {
  struct forward_t* f = forward_find(sensor);
  size_t i;

  if (f == NULL) return;

  /* Remove duplicates */
  for (i = 0; i < CONNECTION_FORWARD_MAX_SIZE; ++i) {
    if (linkaddr_cmp(&f->hops[i].address, hop_address)) {
      shift_left(f, i);
    }
  }

  /* Add */
  shift_right(f);
  linkaddr_copy(&f->hops[0].address, hop_address);
  f->hops[0].distance = hop_distance;

  /* Print */
  print_forwardings();
}

void forward_remove(const linkaddr_t* sensor) {
  struct forward_t* f = forward_find(sensor);

  if (f == NULL) return;

  LOG_WARN("Removing hop %02x:%02x with distance %u for sensor %02x:%02x",
           f->hops[0].address.u8[0], f->hops[0].address.u8[1],
           f->hops[0].distance, sensor->u8[0], sensor->u8[1]);

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
    if (linkaddr_cmp(hop_address, &f->hops[i].address)) break;
  }
  if (i >= CONNECTION_FORWARD_MAX_SIZE) return;

  LOG_WARN("Removing hop %02x:%02x at %d with distance %u for sensor %02x:%02x",
           f->hops[0].address.u8[0], f->hops[0].address.u8[1], i,
           f->hops[0].distance, sensor->u8[0], sensor->u8[1]);

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
    if (linkaddr_cmp(&f->hops[i].address, &linkaddr_null)) break;
    length += 1;
  }

  return length;
}

void forward_sort(const linkaddr_t* sensor) {
  struct forward_t* f = forward_find(sensor);
  struct forward_hop_t tmp;
  size_t i;
  size_t j;
  if (f == NULL) return;

  for (i = 0; i < CONNECTION_UC_BUFFER_MAX_SEND; ++i) {
    for (j = i + 1; j < CONNECTION_UC_BUFFER_MAX_SEND; ++j) {
      if (f->hops[i].distance > f->hops[j].distance) {
        /* Copy tmp */
        linkaddr_copy(&tmp.address, &f->hops[i].address);
        tmp.distance = f->hops[i].distance;
        /* j in i */
        linkaddr_copy(&f->hops[i].address, &f->hops[j].address);
        f->hops[i].distance = f->hops[j].distance;
        /* tmp in j */
        linkaddr_copy(&f->hops[j].address, &tmp.address);
        f->hops[j].distance = tmp.distance;
      }
    }
  }

  /* Print */
  print_forwardings();
}

/* --- RESET --- */
static void reset(void) {
  size_t i;
  size_t j;

  for (i = 0; i < NUM_SENSORS; ++i) {
    linkaddr_copy(&forwardings[i].sensor, &SENSORS[i]);
    for (j = 0; j < CONNECTION_FORWARD_MAX_SIZE; ++j) {
      linkaddr_copy(&forwardings[i].hops[j].address, &linkaddr_null);
      forwardings[i].hops[j].distance = UINT8_MAX;
    }
  }
}

static void shift_right(struct forward_t* f) {
  size_t i;

  if (f == NULL) return;

  for (i = CONNECTION_FORWARD_MAX_SIZE - 1; i > 0; --i) {
    linkaddr_copy(&f->hops[i].address, &f->hops[i - 1].address);
    f->hops[i].distance = f->hops[i - 1].distance;
  }

  linkaddr_copy(&f->hops[0].address, &linkaddr_null);
  f->hops[0].distance = UINT8_MAX;
}

static void shift_left(struct forward_t* f, size_t from) {
  size_t i;

  if (f == NULL) return;

  for (i = from; i < CONNECTION_FORWARD_MAX_SIZE - 1; ++i) {
    linkaddr_copy(&f->hops[i].address, &f->hops[i + 1].address);
    f->hops[i].distance = f->hops[i + 1].distance;
  }

  linkaddr_copy(&f->hops[i].address, &linkaddr_null);
  f->hops[i].distance = UINT8_MAX;
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
      printf("{ address: %02x:%02x, distance: %u } ", f->hops[j].address.u8[0],
             f->hops[j].address.u8[1], f->hops[j].distance);
    }
    printf("] } ");
  }
}
