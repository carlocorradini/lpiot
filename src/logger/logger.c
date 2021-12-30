#include "logger.h"

#include <stdarg.h>

#include "config/config.h"

/**
 * @brief Current log level.
 */
static enum log_level_t log_level = LOGGER_LEVEL;

/**
 * @brief String representation of log levels.
 */
static const char const* log_level_strings[] = {"TRACE", "DEBUG", "INFO",
                                                "WARN",  "ERROR", "FATAL"};

void logger_log(enum log_level_t level, const char* file, int line,
                const char* fmt, ...) {
  va_list arg;

  if (!logger_is_enabled(level)) return; /* Disabled level */

  va_start(arg, fmt);
  printf("%-5s %s:%d: ", log_level_strings[level], file, line);
  vprintf(fmt, arg);
  printf("\n");
  va_end(arg);
}

void logger_set_level(enum log_level_t level) { log_level = level; }

enum log_level_t logger_get_level(void) { return log_level; }

bool logger_is_enabled(enum log_level_t level) { return log_level <= level; }