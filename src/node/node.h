#ifndef _NODE_H_
#define _NODE_H_

/**
 * @brief Node roles.
 */
typedef enum {
  /**
   * @brief Controller node.
   */
  NODE_ROLE_CONTROLLER,
  /**
   * @brief Sensor/Actuator node.
   */
  NODE_ROLE_SENSOR_ACTUATOR,
  /**
   * @brief Forwarder node.
   */
  NODE_ROLE_FORWARDER
} node_role_t;

/**
 * @brief Command types.
 */
typedef enum {
  /**
   * @brief Don't do anything (ignore).
   */
  COMMAND_TYPE_NONE,
  /**
   * @brief Sensed value should go to 0, and the threshold back to normal.
   */
  COMMAND_TYPE_RESET,
  /**
   * @brief Sensed value should not be modified, but the threshold should be
   * increased.
   */
  COMMAND_TYPE_THRESHOLD
} command_type_t;

#endif