#ifndef _CONNECTION_UC_BUFFER_H_
#define _CONNECTION_UC_BUFFER_H_

#include <net/linkaddr.h>
#include <net/packetbuf.h>
#include <stdbool.h>
#include <sys/types.h>

#include "connection.h"

/**
 * @brief Unicast buffer entry.
 * Cache structure for a unicast message.
 */
struct uc_buffer_t {
  /* Free entry flag. */
  bool free;
  /* Header. */
  struct unicast_hdr_t header;
  /* Receiver address. */
  linkaddr_t receiver;
  /* Data in byte. */
  uint8_t data[PACKETBUF_SIZE];
  /* Data length in byte. */
  uint16_t data_len;
  /* Number of times the packet has been sent. */
  uint8_t num_send;
  /* Flag to know if this packet has a last chance. */
  bool last_chance;
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
 * @param header Header.
 * @param receiver Receiver address.
 * @return true Entry added.
 * @return false Entry not added.
 */
bool uc_buffer_add(const struct unicast_hdr_t *header,
                   const linkaddr_t *receiver);

/**
 * @brief Remove first entry in the unicast buffer.
 * A call to this function cause a shift in the buffer.
 */
void uc_buffer_remove(void);

/**
 * @brief Return first message in buffer.
 * Note that the buffered message could be invalid, check free == false to be
 * sure that the entry is valid.
 *
 * @return First buffered message.
 */
struct uc_buffer_t *uc_buffer_first(void);

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

#endif
