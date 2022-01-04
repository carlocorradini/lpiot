#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define __FILENAME__ \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/* Log a trace message. */
#define LOG_TRACE(fmt, ...) \
  logger_log(LOG_LEVEL_TRACE, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
/* Log a debug message. */
#define LOG_DEBUG(fmt, ...) \
  logger_log(LOG_LEVEL_DEBUG, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
/* Log an info message. */
#define LOG_INFO(fmt, ...) \
  logger_log(LOG_LEVEL_INFO, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
/* Log a warn message. */
#define LOG_WARN(fmt, ...) \
  logger_log(LOG_LEVEL_WARN, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
/* Log an error message. */
#define LOG_ERROR(fmt, ...) \
  logger_log(LOG_LEVEL_ERROR, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
/* Log a fatal message. */
#define LOG_FATAL(fmt, ...) \
  logger_log(LOG_LEVEL_FATAL, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log levels.
 */
enum log_level_t {
  /* Trace level. */
  LOG_LEVEL_TRACE,
  /* Debug level. */
  LOG_LEVEL_DEBUG,
  /* Info level. */
  LOG_LEVEL_INFO,
  /* Warn level. */
  LOG_LEVEL_WARN,
  /* Error level. */
  LOG_LEVEL_ERROR,
  /* Fatal level. */
  LOG_LEVEL_FATAL,
  /* Disable logging. */
  LOG_LEVEL_DISABLED,
};

/**
 * @brief Log a message.
 *
 * @param level Log level.
 * @param file Calling file.
 * @param line Calling file's line.
 * @param fmt C string that contains the text to be written to the stream.
 * @param ... Additional arguments.
 */
void logger_log(enum log_level_t level, const char* file, int line,
                const char* fmt, ...);

/**
 * @brief Set the log level.
 * Message levels lower than the value will be discarded.
 * Default level is INFO.
 *
 * @param level Log level to set.
 */
void logger_set_level(enum log_level_t level);

/**
 * @brief Return the current log level.
 *
 * @return Current log level.
 */
enum log_level_t logger_get_level(void);

/**
 * @brief Check if logging for the given level is active.
 *
 * @param level Level to check.
 * @return true Logging for level is enabled.
 * @return false Loggin for level is disabled.
 */
bool logger_is_enabled(enum log_level_t level);

/**
 * @brief Enable or disable newline character '\n'.
 * The '\n' character is printed after the log message.
 *
 * @param enable True enable, false disable '\n'.
 */
void logger_set_newline(bool enable);

#endif
