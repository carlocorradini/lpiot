#include "forwarder.h"

#include "config/config.h"
#include "etc/etc.h"

void forwarder_init(void) {
  /* Open ETC connection */
  etc_open(CONNECTION_CHANNEL, NULL);
}
