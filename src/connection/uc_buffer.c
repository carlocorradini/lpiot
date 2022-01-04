#include "uc_buffer.h"

#include "config/config.h"
#include "logger/logger.h"

static struct uc_buffer_t buffer[CONNECTION_UC_BUFFER_SIZE];

/**
 * @brief Reset the ith entry in the buffer.
 *
 * @param index Index of the entry.
 */
static void reset_idx(size_t index);

/**
 * @brief Reset buffer.
 */
static void reset(void);

/**
 * @brief Shift buffer to left replacing the first element with the one the
 * right.
 * The last entry is resetted.
 * Example:
 * [REMOVE 2 3] -> [2 3 ?]
 * The ? entry is resetted.
 */
static void shift_left(void);

/* --- --- */
void uc_buffer_init(void) { reset(); }

void uc_buffer_terminate(void) { reset(); }

bool uc_buffer_add(enum unicast_msg_type_t type, const linkaddr_t *receiver) {
  size_t i;

  /* Discard null receiver */
  if (linkaddr_cmp(receiver, &linkaddr_null)) return false;

  /* Find a (possible) space */
  for (i = 0; i < CONNECTION_UC_BUFFER_SIZE; ++i) {
    if (buffer[i].free) break;
  }

  /* Check buffer availability */
  if (i >= CONNECTION_UC_BUFFER_SIZE) {
    LOG_ERROR(
        "Unable to save unicast message in buffer: "
        "{ type %d, receiver: %02x:%02x }",
        type, receiver->u8[0], receiver->u8[1]);
    return false;
  }

  /* Save */
  buffer[i].free = false;
  linkaddr_copy(&buffer[i].receiver, receiver);
  buffer[i].msg_type = type;
  packetbuf_copyto(buffer[i].data);
  buffer[i].data_len = packetbuf_datalen();
  buffer[i].num_retry_left = CONNECTION_UC_BUFFER_MAX_RETRY;

  return true;
}

size_t uc_buffer_length(void) {
  size_t i;
  size_t length = 0;

  for (i = 0; i < CONNECTION_UC_BUFFER_SIZE; ++i) {
    if (buffer[i].free) break;
    length += 1;
  }

  return length;
}

bool uc_bufffer_is_empty(void) { return uc_buffer_length() == 0; }

void uc_buffer_fail(void) { shift_left(); }

void uc_buffer_success(void) { shift_left(); }

const struct uc_buffer_t *uc_buffer_retry(void) {
  struct uc_buffer_t *buf = &buffer[0];

  if (!uc_buffer_can_retry()) return NULL;

  buf->num_retry_left -= 1;

  if (buf->msg_type == UNICAST_MSG_TYPE_COLLECT) {
    buf->num_retry_left += 1;
    linkaddr_copy(&buf->receiver, &connection_get_conn()->parent_node);
  }

  return &buffer[0];
}

bool uc_buffer_can_retry(void) {
  return !buffer[0].free && buffer[0].num_retry_left > 0;
}

/* --- RESET --- */
static void reset_idx(size_t index) {
  if (index < 0 || index >= CONNECTION_UC_BUFFER_SIZE) return;

  buffer[index].free = true;
}

static void reset(void) {
  size_t i;
  for (i = 0; i < CONNECTION_UC_BUFFER_SIZE; ++i) {
    reset_idx(i);
  }
}

static void shift_left(void) {
  size_t i;
  for (i = 0; i < CONNECTION_UC_BUFFER_SIZE - 1; ++i) {
    buffer[i].free = buffer[i + 1].free;
    linkaddr_copy(&buffer[i].receiver, &buffer[i + 1].receiver);
    buffer[i].msg_type = buffer[i + 1].msg_type;
    memcpy(buffer[i].data, buffer[i + 1].data, buffer[i + 1].data_len);
    buffer[i].data_len = buffer[i + 1].data_len;
    buffer[i].num_retry_left = buffer[i + 1].num_retry_left;
  }

  reset_idx(i);
}
