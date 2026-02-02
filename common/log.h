/*
 * USB Over Network - Logging System
 * Windows-only implementation
 *
 * Thread-safe logging with multiple log levels and output targets
 */

#ifndef LOG_H
#define LOG_H

#include "types.h"
#include "config.h"
#include "error.h"

/* Log Levels */
typedef enum log_level {
    LOG_LEVEL_TRACE = 0,    /* Very detailed debugging */
    LOG_LEVEL_DEBUG = 1,    /* Debug information */
    LOG_LEVEL_INFO = 2,     /* General information */
    LOG_LEVEL_WARN = 3,     /* Warnings */
    LOG_LEVEL_ERROR = 4,    /* Errors */
    LOG_LEVEL_FATAL = 5,    /* Fatal errors */
    LOG_LEVEL_NONE = 6      /* No logging */
} log_level_t;

/* Log Output Targets */
typedef enum log_target {
    LOG_TARGET_NONE = 0x00,
    LOG_TARGET_CONSOLE = 0x01,
    LOG_TARGET_FILE = 0x02,
    LOG_TARGET_DEBUGGER = 0x04,
    LOG_TARGET_ALL = 0x07
} log_target_t;

/* Log Configuration */
typedef struct log_config {
    log_level_t level;
    log_target_t targets;
    bool show_timestamp;
    bool show_level;
    bool show_location;
    bool use_colors;
    char log_file[MAX_PATH];
} log_config_t;

/* Initialize logging system */
error_code_t log_init(const log_config_t *config);

/* Cleanup logging system */
void log_cleanup(void);

/* Set log level */
void log_set_level(log_level_t level);

/* Get current log level */
log_level_t log_get_level(void);

/* Set log targets */
void log_set_targets(log_target_t targets);

/* Set log file */
error_code_t log_set_file(const char *filepath);

/* Close log file */
void log_close_file(void);

/* Core logging function */
void log_write(log_level_t level, const char *file, int line, const char *func, const char *fmt, ...);

/* Hex dump logging */
void log_hexdump(log_level_t level, const char *prefix, const void *data, size_t len);

/* ----- Logging Macros ----- */

/* Check if log level is enabled (for conditional expensive operations) */
#define LOG_ENABLED(level) (log_get_level() <= (level))

/* Main logging macros */
#define LOG_TRACE(...)  log_write(LOG_LEVEL_TRACE, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_DEBUG(...)  log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_INFO(...)   log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_WARN(...)   log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_ERROR(...)  log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOG_FATAL(...)  log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

/* Conditional logging (only evaluate arguments if level is enabled) */
#define LOG_TRACE_IF(cond, ...) do { if ((cond) && LOG_ENABLED(LOG_LEVEL_TRACE)) LOG_TRACE(__VA_ARGS__); } while(0)
#define LOG_DEBUG_IF(cond, ...) do { if ((cond) && LOG_ENABLED(LOG_LEVEL_DEBUG)) LOG_DEBUG(__VA_ARGS__); } while(0)
#define LOG_INFO_IF(cond, ...)  do { if ((cond) && LOG_ENABLED(LOG_LEVEL_INFO))  LOG_INFO(__VA_ARGS__);  } while(0)
#define LOG_WARN_IF(cond, ...)  do { if ((cond) && LOG_ENABLED(LOG_LEVEL_WARN))  LOG_WARN(__VA_ARGS__);  } while(0)
#define LOG_ERROR_IF(cond, ...) do { if ((cond) && LOG_ENABLED(LOG_LEVEL_ERROR)) LOG_ERROR(__VA_ARGS__); } while(0)

/* Hex dump macros */
#define LOG_HEXDUMP_TRACE(prefix, data, len) log_hexdump(LOG_LEVEL_TRACE, prefix, data, len)
#define LOG_HEXDUMP_DEBUG(prefix, data, len) log_hexdump(LOG_LEVEL_DEBUG, prefix, data, len)

/* Function entry/exit logging */
#define LOG_FUNC_ENTER()    LOG_TRACE(">>> %s", __FUNCTION__)
#define LOG_FUNC_EXIT()     LOG_TRACE("<<< %s", __FUNCTION__)
#define LOG_FUNC_EXIT_RC(rc) LOG_TRACE("<<< %s: rc=%d", __FUNCTION__, (int)(rc))

/* Error logging with error code */
#define LOG_ERR_CODE(code, ...) do { \
    LOG_ERROR(__VA_ARGS__); \
    LOG_ERROR("Error code: 0x%04X (%s)", (code), error_string(code)); \
} while(0)

/* Log Windows error */
#define LOG_WIN32_ERROR(msg) do { \
    DWORD _err = GetLastError(); \
    char _buf[256]; \
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, _err, 0, _buf, sizeof(_buf), NULL); \
    LOG_ERROR("%s: %s (0x%08lX)", (msg), _buf, _err); \
} while(0)

/* Log Winsock error */
#define LOG_WINSOCK_ERROR(msg) do { \
    int _err = WSAGetLastError(); \
    LOG_ERROR("%s: Winsock error %d", (msg), _err); \
} while(0)

/* ----- Debug Macros (only in debug builds) ----- */

#ifdef _DEBUG
    #define DLOG_TRACE(...) LOG_TRACE(__VA_ARGS__)
    #define DLOG_DEBUG(...) LOG_DEBUG(__VA_ARGS__)
    #define DLOG_HEXDUMP(prefix, data, len) LOG_HEXDUMP_DEBUG(prefix, data, len)
#else
    #define DLOG_TRACE(...) ((void)0)
    #define DLOG_DEBUG(...) ((void)0)
    #define DLOG_HEXDUMP(prefix, data, len) ((void)0)
#endif

/* ----- Assertion with Logging ----- */

#ifdef _DEBUG
#define LOG_ASSERT(cond) do { \
    if (!(cond)) { \
        LOG_FATAL("Assertion failed: %s at %s:%d", #cond, __FILE__, __LINE__); \
        __debugbreak(); \
    } \
} while(0)
#else
#define LOG_ASSERT(cond) ((void)0)
#endif

#endif /* LOG_H */
