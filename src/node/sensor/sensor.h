#ifndef _NODE_SENSOR_H_
#define _NODE_SENSOR_H_

#include <stddef.h>
#include <sys/types.h>

/**
 * @brief Initialize Sensor/Actuator node.
 *
 * @param index Sensor index.
 */
void sensor_init(size_t index);

#endif
