#ifndef _NODE_H_
#define _NODE_H_

/**
 * @brief Node roles.
 */
enum node_role_t {
  /**
   * @brief Unknown role.
   */
  NODE_ROLE_UNKNOWN,
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
};

/**
 * @brief Command types.
 */
enum command_type_t {
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
};

/**
 * @brief Return the role of the node.
 *
 * @return Node role.
 */
enum node_role_t node_get_role(void);

/**
 * @brief Return the role name of the node.
 *
 * @return Node role name.
 */
const char const* node_get_role_name(void);

#endif
