/*
 * USB Over Network - Logging System Implementation
 * Windows-only implementation
 */

#include "log.h"
#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* Console color codes for Windows */
#define COLOR_RESET     7   /* Default */
#define COLOR_TRACE     8   /* Dark gray */
#define COLOR_DEBUG     11  /* Cyan */
#define COLOR_INFO      10  /* Green */
#define COLOR_WARN      14  /* Yellow */
#define COLOR_ERROR     12  /* Red */
#define COLOR_FATAL     13  /* Magenta */

/* Log level names */
static const char* level_names[] = {
    "TRACE",
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL"
};

/* Log level colors */
static const int level_colors[] = {
    COLOR_TRACE,
    COLOR_DEBUG,
    COLOR_INFO,
    COLOR_WARN,
    COLOR_ERROR,
    COLOR_FATAL
};

/* Global log configuration */
static struct {
    log_config_t config;
    HANDLE console_handle;
    FILE *log_file;
    mutex_t mutex;
    bool initialized;
} g_log = {
    .config = {
        .level = LOG_LEVEL_INFO,
        .targets = LOG_TARGET_CONSOLE,
        .show_timestamp = true,
        .show_level = true,
        .show_location = false,
        .use_colors = true,
        .log_file = {0}
    },
    .console_handle = INVALID_HANDLE_VALUE,
    .log_file = NULL,
    .initialized = false
};

/*
 * Initialize logging system
 */
error_code_t log_init(const log_config_t *config) {
    if (g_log.initialized) {
        return ERR_ALREADY_INITIALIZED;
    }

    /* Initialize mutex */
    if (mutex_init(&g_log.mutex) != 0) {
        return ERR_MUTEX_INIT;
    }

    /* Apply configuration */
    if (config != NULL) {
        memcpy(&g_log.config, config, sizeof(log_config_t));
    }

    /* Get console handle for color output */
    if (g_log.config.targets & LOG_TARGET_CONSOLE) {
        g_log.console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    }

    /* Open log file if specified */
    if ((g_log.config.targets & LOG_TARGET_FILE) && g_log.config.log_file[0] != '\0') {
        g_log.log_file = fopen(g_log.config.log_file, "a");
        if (g_log.log_file == NULL) {
            mutex_destroy(&g_log.mutex);
            return ERR_FILE_OPEN;
        }
    }

    g_log.initialized = true;
    LOG_INFO("%s v%s logging initialized", APP_NAME, APP_VERSION);
    return ERR_SUCCESS;
}

/*
 * Cleanup logging system
 */
void log_cleanup(void) {
    if (!g_log.initialized) {
        return;
    }

    LOG_INFO("Logging shutdown");

    if (g_log.log_file != NULL) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }

    mutex_destroy(&g_log.mutex);
    g_log.initialized = false;
}

/*
 * Set log level
 */
void log_set_level(log_level_t level) {
    g_log.config.level = level;
}

/*
 * Get current log level
 */
log_level_t log_get_level(void) {
    return g_log.config.level;
}

/*
 * Set log targets
 */
void log_set_targets(log_target_t targets) {
    g_log.config.targets = targets;
}

/*
 * Set log file
 */
error_code_t log_set_file(const char *filepath) {
    if (filepath == NULL) {
        return ERR_INVALID_PARAM;
    }

    mutex_lock(&g_log.mutex);

    /* Close existing file */
    if (g_log.log_file != NULL) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }

    /* Open new file */
    strncpy(g_log.config.log_file, filepath, MAX_PATH - 1);
    g_log.log_file = fopen(filepath, "a");

    if (g_log.log_file == NULL) {
        mutex_unlock(&g_log.mutex);
        return ERR_FILE_OPEN;
    }

    g_log.config.targets |= LOG_TARGET_FILE;
    mutex_unlock(&g_log.mutex);
    return ERR_SUCCESS;
}

/*
 * Close log file
 */
void log_close_file(void) {
    mutex_lock(&g_log.mutex);
    if (g_log.log_file != NULL) {
        fclose(g_log.log_file);
        g_log.log_file = NULL;
    }
    g_log.config.targets &= ~LOG_TARGET_FILE;
    mutex_unlock(&g_log.mutex);
}

/*
 * Set console color
 */
static void set_console_color(int color) {
    if (g_log.console_handle != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(g_log.console_handle, (WORD)color);
    }
}

/*
 * Get current timestamp string
 */
static void get_timestamp(char *buffer, size_t size) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

/*
 * Extract filename from path
 */
static const char* get_filename(const char *path) {
    const char *name = strrchr(path, '\\');
    if (name == NULL) {
        name = strrchr(path, '/');
    }
    return (name != NULL) ? name + 1 : path;
}

/*
 * Core logging function
 */
void log_write(log_level_t level, const char *file, int line, const char *func, const char *fmt, ...) {
    /* Check log level */
    if (level < g_log.config.level || g_log.config.targets == LOG_TARGET_NONE) {
        return;
    }

    char timestamp[48] = {0};  /* fits "%04d-%02d-%02d %02d:%02d:%02d.%03d" + slack */
    char message[LOG_BUFFER_SIZE];
    char full_message[LOG_BUFFER_SIZE + 256];
    va_list args;

    /* Format user message */
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* Get timestamp */
    if (g_log.config.show_timestamp) {
        get_timestamp(timestamp, sizeof(timestamp));
    }

    /* Build full message */
    char *p = full_message;
    size_t remaining = sizeof(full_message);
    int written;

    if (g_log.config.show_timestamp) {
        written = snprintf(p, remaining, "[%s] ", timestamp);
        p += written;
        remaining -= written;
    }

    if (g_log.config.show_level) {
        written = snprintf(p, remaining, "[%s] ", level_names[level]);
        p += written;
        remaining -= written;
    }

    if (g_log.config.show_location) {
        written = snprintf(p, remaining, "[%s:%d %s] ", get_filename(file), line, func);
        p += written;
        remaining -= written;
    }

    snprintf(p, remaining, "%s", message);

    /* Lock for thread safety */
    if (g_log.initialized) {
        mutex_lock(&g_log.mutex);
    }

    /* Output to console */
    if (g_log.config.targets & LOG_TARGET_CONSOLE) {
        if (g_log.config.use_colors) {
            set_console_color(level_colors[level]);
        }
        fprintf(stderr, "%s\n", full_message);
        if (g_log.config.use_colors) {
            set_console_color(COLOR_RESET);
        }
        fflush(stderr);
    }

    /* Output to file */
    if ((g_log.config.targets & LOG_TARGET_FILE) && g_log.log_file != NULL) {
        fprintf(g_log.log_file, "%s\n", full_message);
        fflush(g_log.log_file);
    }

    /* Output to debugger */
    if (g_log.config.targets & LOG_TARGET_DEBUGGER) {
        char debug_msg[LOG_BUFFER_SIZE + 258];
        snprintf(debug_msg, sizeof(debug_msg), "%s\n", full_message);
        OutputDebugStringA(debug_msg);
    }

    if (g_log.initialized) {
        mutex_unlock(&g_log.mutex);
    }
}

/*
 * Hex dump logging
 */
void log_hexdump(log_level_t level, const char *prefix, const void *data, size_t len) {
    if (level < g_log.config.level || data == NULL || len == 0) {
        return;
    }

    const uint8_t *bytes = (const uint8_t *)data;
    char line[128];
    char hex_part[50];
    char ascii_part[17];
    size_t offset = 0;

    log_write(level, __FILE__, __LINE__, __FUNCTION__,
        "%s: %zu bytes", prefix ? prefix : "Data", len);

    while (offset < len) {
        size_t line_len = (len - offset > 16) ? 16 : (len - offset);
        char *hex_p = hex_part;

        /* Build hex part */
        for (size_t i = 0; i < 16; i++) {
            if (i < line_len) {
                hex_p += sprintf(hex_p, "%02X ", bytes[offset + i]);
                ascii_part[i] = (bytes[offset + i] >= 32 && bytes[offset + i] < 127)
                    ? bytes[offset + i] : '.';
            } else {
                hex_p += sprintf(hex_p, "   ");
                ascii_part[i] = ' ';
            }
            if (i == 7) {
                *hex_p++ = ' ';
            }
        }
        ascii_part[16] = '\0';

        snprintf(line, sizeof(line), "  %04zX: %-49s |%s|", offset, hex_part, ascii_part);

        /* Lock and output */
        if (g_log.initialized) {
            mutex_lock(&g_log.mutex);
        }

        if (g_log.config.targets & LOG_TARGET_CONSOLE) {
            fprintf(stderr, "%s\n", line);
        }
        if ((g_log.config.targets & LOG_TARGET_FILE) && g_log.log_file != NULL) {
            fprintf(g_log.log_file, "%s\n", line);
        }
        if (g_log.config.targets & LOG_TARGET_DEBUGGER) {
            char debug_msg[150];
            snprintf(debug_msg, sizeof(debug_msg), "%s\n", line);
            OutputDebugStringA(debug_msg);
        }

        if (g_log.initialized) {
            mutex_unlock(&g_log.mutex);
        }

        offset += line_len;
    }
}
