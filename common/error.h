/*
 * USB Over Network - Error Handling
 * Windows-only implementation
 *
 * Error codes, error handling macros, and error reporting
 */

#ifndef ERROR_H
#define ERROR_H

#include "types.h"

/* Error Code Categories:
 * 0x0000       - Success
 * 0x0001-0x00FF - General errors
 * 0x0100-0x01FF - Network errors
 * 0x0200-0x02FF - USB errors
 * 0x0300-0x03FF - Protocol errors
 * 0x0400-0x04FF - VHCI errors
 * 0x0500-0x05FF - System errors
 */

typedef enum error_code {
    /* Success */
    ERR_SUCCESS = 0,

    /* General Errors (0x0001-0x00FF) */
    ERR_GENERAL = 0x0001,
    ERR_OUT_OF_MEMORY,
    ERR_INVALID_PARAM,
    ERR_NOT_FOUND,
    ERR_ALREADY_EXISTS,
    ERR_NOT_SUPPORTED,
    ERR_TIMEOUT,
    ERR_BUSY,
    ERR_PERMISSION_DENIED,
    ERR_IO_ERROR,
    ERR_BUFFER_TOO_SMALL,
    ERR_INVALID_STATE,
    ERR_CANCELLED,
    ERR_NOT_INITIALIZED,
    ERR_ALREADY_INITIALIZED,

    /* Network Errors (0x0100-0x01FF) */
    ERR_NETWORK_INIT = 0x0100,
    ERR_SOCKET_CREATE,
    ERR_SOCKET_BIND,
    ERR_SOCKET_LISTEN,
    ERR_SOCKET_ACCEPT,
    ERR_SOCKET_CONNECT,
    ERR_SOCKET_SEND,
    ERR_SOCKET_RECV,
    ERR_SOCKET_CLOSE,
    ERR_SOCKET_OPTION,
    ERR_HOST_NOT_FOUND,
    ERR_CONNECTION_REFUSED,
    ERR_CONNECTION_RESET,
    ERR_CONNECTION_TIMEOUT,
    ERR_NETWORK_UNREACHABLE,
    ERR_ADDRESS_IN_USE,

    /* USB Errors (0x0200-0x02FF) */
    ERR_USB_INIT = 0x0200,
    ERR_USB_NOT_FOUND,
    ERR_USB_ACCESS_DENIED,
    ERR_USB_BUSY,
    ERR_USB_NO_DEVICE,
    ERR_USB_CLAIM_FAILED,
    ERR_USB_RELEASE_FAILED,
    ERR_USB_IO_FAILED,
    ERR_USB_STALL,
    ERR_USB_OVERFLOW,
    ERR_USB_PIPE_ERROR,
    ERR_USB_INTERRUPTED,
    ERR_USB_NO_MEMORY,
    ERR_USB_INVALID_ENDPOINT,
    ERR_USB_INVALID_INTERFACE,
    ERR_USB_DETACHED,
    ERR_USB_DESCRIPTOR_ERROR,
    ERR_DEVICE_BUSY,

    /* Protocol Errors (0x0300-0x03FF) */
    ERR_PROTOCOL_VERSION = 0x0300,
    ERR_PROTOCOL_INVALID,
    ERR_PROTOCOL_UNSUPPORTED,
    ERR_PROTOCOL_SEQUENCE,
    ERR_PROTOCOL_HEADER,
    ERR_PROTOCOL_DATA,
    ERR_PROTOCOL_CHECKSUM,
    ERR_PROTOCOL_STATE,

    /* VHCI Errors (0x0400-0x04FF) */
    ERR_VHCI_INIT = 0x0400,
    ERR_VHCI_NOT_FOUND,
    ERR_VHCI_ATTACH_FAILED,
    ERR_VHCI_DETACH_FAILED,
    ERR_VHCI_NO_FREE_PORT,
    ERR_VHCI_INVALID_PORT,
    ERR_VHCI_DRIVER_ERROR,

    /* System Errors (0x0500-0x05FF) */
    ERR_SYSTEM_INIT = 0x0500,
    ERR_THREAD_CREATE,
    ERR_THREAD_JOIN,
    ERR_MUTEX_INIT,
    ERR_MUTEX_LOCK,
    ERR_FILE_OPEN,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_REGISTRY_ACCESS,

    /* Maximum error code marker */
    ERR_MAX
} error_code_t;

/* Error context structure for detailed error information */
typedef struct error_context {
    error_code_t code;
    DWORD system_error;         /* Windows GetLastError() */
    int socket_error;           /* WSAGetLastError() */
    char message[256];          /* Custom error message */
    const char *file;           /* Source file */
    int line;                   /* Source line */
    const char *function;       /* Function name */
} error_context_t;

/* Thread-local error context */
#ifdef _MSC_VER
    extern __declspec(thread) error_context_t g_last_error;
#else
    extern __thread error_context_t g_last_error;
#endif

/* Get error string from error code */
const char* error_string(error_code_t code);

/* Get detailed error message including system error */
const char* error_detail(void);

/* Set error context */
void error_set(error_code_t code, const char *file, int line, const char *func, const char *fmt, ...);

/* Clear error context */
void error_clear(void);

/* Get last error code */
error_code_t error_get_code(void);

/* Get last system error code */
DWORD error_get_system(void);

/* Check if error code is in specific category */
#define ERROR_IS_NETWORK(e)     (((e) >= 0x0100) && ((e) < 0x0200))
#define ERROR_IS_USB(e)         (((e) >= 0x0200) && ((e) < 0x0300))
#define ERROR_IS_PROTOCOL(e)    (((e) >= 0x0300) && ((e) < 0x0400))
#define ERROR_IS_VHCI(e)        (((e) >= 0x0400) && ((e) < 0x0500))
#define ERROR_IS_SYSTEM(e)      (((e) >= 0x0500) && ((e) < 0x0600))

/* ----- Error Handling Macros ----- */

/* Set error with current location */
#define SET_ERROR(code, ...) \
    error_set((code), __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

/* Set error and return */
#define RETURN_ERROR(code, ...) do { \
    SET_ERROR(code, __VA_ARGS__); \
    return (code); \
} while(0)

/* Set error and return NULL */
#define RETURN_ERROR_NULL(code, ...) do { \
    SET_ERROR(code, __VA_ARGS__); \
    return NULL; \
} while(0)

/* Check condition, set error and return if false */
#define CHECK_ERROR(cond, code, ...) do { \
    if (!(cond)) { \
        RETURN_ERROR(code, __VA_ARGS__); \
    } \
} while(0)

/* Check condition, set error and return NULL if false */
#define CHECK_ERROR_NULL(cond, code, ...) do { \
    if (!(cond)) { \
        RETURN_ERROR_NULL(code, __VA_ARGS__); \
    } \
} while(0)

/* Check condition, set error and goto label if false */
#define CHECK_ERROR_GOTO(cond, code, label, ...) do { \
    if (!(cond)) { \
        SET_ERROR(code, __VA_ARGS__); \
        goto label; \
    } \
} while(0)

/* Check pointer for NULL */
#define CHECK_NULL(ptr, code) \
    CHECK_ERROR((ptr) != NULL, code, "NULL pointer: %s", #ptr)

/* Check pointer for NULL, return NULL */
#define CHECK_NULL_RET_NULL(ptr, code) \
    CHECK_ERROR_NULL((ptr) != NULL, code, "NULL pointer: %s", #ptr)

/* Check memory allocation */
#define CHECK_ALLOC(ptr) \
    CHECK_ERROR((ptr) != NULL, ERR_OUT_OF_MEMORY, "Memory allocation failed")

/* Check memory allocation, return NULL */
#define CHECK_ALLOC_NULL(ptr) \
    CHECK_ERROR_NULL((ptr) != NULL, ERR_OUT_OF_MEMORY, "Memory allocation failed")

/* Check socket operation */
#define CHECK_SOCKET(result, code) \
    CHECK_ERROR((result) != SOCKET_ERROR_VAL, code, "Socket error: %d", WSAGetLastError())

/* Check Windows API result */
#define CHECK_WIN32(result, code) \
    CHECK_ERROR((result), code, "Windows error: %lu", GetLastError())

/* Check return code */
#define CHECK_RET(ret) do { \
    error_code_t _ret = (ret); \
    if (_ret != ERR_SUCCESS) { \
        return _ret; \
    } \
} while(0)

/* Assert with error */
#ifdef _DEBUG
#define ASSERT_ERROR(cond, code, ...) do { \
    if (!(cond)) { \
        SET_ERROR(code, __VA_ARGS__); \
        __debugbreak(); \
    } \
} while(0)
#else
#define ASSERT_ERROR(cond, code, ...) ((void)0)
#endif

/* Success check */
#define IS_SUCCESS(code) ((code) == ERR_SUCCESS)
/* Avoid conflict with Windows IS_ERROR macro */
#ifdef IS_ERROR
#undef IS_ERROR
#endif
#define IS_ERROR(code) ((code) != ERR_SUCCESS)

#endif /* ERROR_H */
