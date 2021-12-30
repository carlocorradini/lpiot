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

/**
 * @brief Return last sensed value.
 *
 * @return Last value.
 */
uint32_t sensor_get_value(void);

/**
 * @brief Return current threshold.
 *
 * @return Current threshold.
 */
uint32_t sensor_get_threshold(void);

#endif