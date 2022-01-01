#ifndef _NODE_H_
#define _NODE_H_

/**
 * @brief Node roles.
 */
enum node_role_t {
  /* Unknown role. */
  NODE_ROLE_UNKNOWN,
  /* Controller node. */
  NODE_ROLE_CONTROLLER,
  /* Sensor/Actuator node. */
  NODE_ROLE_SENSOR_ACTUATOR,
  /* Forwarder node. */
  NODE_ROLE_FORWARDER
};

/**
 * @brief Command types.
 */
enum command_type_t {
  /* Don't do anything (ignore). */
  COMMAND_TYPE_NONE,
  /* Sensed value should go to 0, and the threshold back to normal. */
  COMMAND_TYPE_RESET,
  /* Sensed value should not be modified, but the threshold should be increased.
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
