#include "connection.h"

#include <net/rime/broadcast.h>
#include <net/rime/unicast.h>

#include "beacon/beacon.h"

const struct connection_t *const best_conn;

/* --- BROADCAST --- */
/**
 * @brief Broadcast connection object.
 */
static struct broadcast_conn bc_conn;

/**
 * @brief Broadcast receive callback.
 *
 * @param bc_conn Broadcast connection.
 * @param sender Address of the sender node.
 */
static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender);

/**
 * @brief Broadcast callback structure.
 */
static const struct broadcast_callbacks bc_cb = {.recv = bc_recv_cb,
                                                 .sent = NULL};

/* --- UNICAST --- */
/**
 * @brief Unicast connection object.
 */
static struct unicast_conn uc_conn;

/* --- --- */
void connection_init(node_role_t node_role, uint16_t channel) {
  /* Open the underlying Rime primitive */
  broadcast_open(&bc_conn, channel, &bc_cb);
  beacon_init(best_conn, &bc_conn, node_role);
}

static void bc_recv_cb(struct broadcast_conn *bc_conn,
                       const linkaddr_t *sender) {
  beacon_bc_recv_cb(bc_conn, sender);
}
