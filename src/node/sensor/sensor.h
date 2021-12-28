#ifndef _NODE_SENSOR_H_
#define _NODE_SENSOR_H_

#include <sys/types.h>

#include "etc/etc.h"

/**
 * @brief Initialize Sensor/Actuator node.
 *
 * @param conn Pointer to an ETC connection object.
 * @param sensor_index Sensor index.
 */
void sensor_init(struct etc_conn_t *conn, uint sensor_index);

#endif