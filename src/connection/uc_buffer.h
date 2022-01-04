#ifndef _UC_BUFFER_
#define _UC_BUFFER_

#include <net/linkaddr.h>
#include <net/packetbuf.h>
#include <stdbool.h>
#include <sys/types.h>

#include "connection/connection.h"

#define CONNECTION_UC_BUFFER_SIZE 3
#define CONNECTION_UC_MAX_RETRY 1

/**
 * @brief Unicast buffer entry.
 * Cache structure for a unicast message.
 */
struct uc_buffer_t {
  /* Free entry flag. */
  bool free;
  /* Receiver address. */
  linkaddr_t receiver;
  /* Message type. */
  enum unicast_msg_type_t msg_type;
  /* Data in byte. */
  uint8_t data[PACKETBUF_SIZE];
  /* Data length in byte. */
  uint16_t data_len;
  /* Number of retry attempts left. */
  uint8_t num_retry_left;
};

/**
 * @brief Initialize unicast buffer.
 */
void uc_buffer_init(void);

/**
 * @brief Terminate unicast buffer.
 */
void uc_buffer_terminate(void);

/**
 * @brief Add an entry to the unicast buffer.
 * Note that if the buffer is full no entry could be added and false is
 * returned.
 *
 * @param type Message type.
 * @param receiver Receiver address.
 * @return true Entry added.
 * @return false Entry not added.
 */
bool uc_buffer_add(enum unicast_msg_type_t type, const linkaddr_t *receiver);

/**
 * @brief Current number of unicast messages in the buffer.
 *
 * @return Messages in buffer.
 */
size_t uc_buffer_length(void);

/**
 * @brief Check if unicast buffer is empty.
 *
 * @return true Empty.
 * @return false Not empty.
 */
bool uc_bufffer_is_empty(void);

/**
 * @brief Unicast message defitely could not be sent.
 * Remove the entry in the buffer.
 */
void uc_buffer_fail(void);

/**
 * @brief Unicast message sent.
 * Remove the entry in the buffer.
 */
void uc_buffer_success(void);

/**
 * @brief Return first message to be (re)sent.
 * Note that calling this fucntion change the internal state of the buffer so
 * the message should be sent.
 * If no buffered message available return NULL.
 *
 * @return First buffered message, NULL otherwise.
 */
const struct uc_buffer_t *uc_buffer_retry(void);

/**
 * @brief Check if there is one message in the buffer.
 *
 * @return true At least one message in the buffer.
 * @return false No messages in the buffer.
 */
bool uc_buffer_can_retry(void);

#endif
