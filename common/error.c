/*
 * USB Over Network - Error Handling Implementation
 * Windows-only implementation
 */

#include "error.h"
#include <stdio.h>
#include <stdarg.h>

/* Thread-local error context */
#ifdef _MSC_VER
__declspec(thread) error_context_t g_last_error = {0};
static __declspec(thread) char g_error_detail_buffer[1024];
#else
__thread error_context_t g_last_error = {0};
static __thread char g_error_detail_buffer[1024];
#endif

/* Error code to string mapping */
static const struct {
    error_code_t code;
    const char *message;
} error_messages[] = {
    /* Success */
    {ERR_SUCCESS, "Success"},

    /* General Errors */
    {ERR_GENERAL, "General error"},
    {ERR_OUT_OF_MEMORY, "Out of memory"},
    {ERR_INVALID_PARAM, "Invalid parameter"},
    {ERR_NOT_FOUND, "Not found"},
    {ERR_ALREADY_EXISTS, "Already exists"},
    {ERR_NOT_SUPPORTED, "Not supported"},
    {ERR_TIMEOUT, "Operation timed out"},
    {ERR_BUSY, "Resource is busy"},
    {ERR_PERMISSION_DENIED, "Permission denied"},
    {ERR_IO_ERROR, "I/O error"},
    {ERR_BUFFER_TOO_SMALL, "Buffer too small"},
    {ERR_INVALID_STATE, "Invalid state"},
    {ERR_CANCELLED, "Operation cancelled"},
    {ERR_NOT_INITIALIZED, "Not initialized"},
    {ERR_ALREADY_INITIALIZED, "Already initialized"},

    /* Network Errors */
    {ERR_NETWORK_INIT, "Network initialization failed"},
    {ERR_SOCKET_CREATE, "Failed to create socket"},
    {ERR_SOCKET_BIND, "Failed to bind socket"},
    {ERR_SOCKET_LISTEN, "Failed to listen on socket"},
    {ERR_SOCKET_ACCEPT, "Failed to accept connection"},
    {ERR_SOCKET_CONNECT, "Failed to connect"},
    {ERR_SOCKET_SEND, "Failed to send data"},
    {ERR_SOCKET_RECV, "Failed to receive data"},
    {ERR_SOCKET_CLOSE, "Failed to close socket"},
    {ERR_SOCKET_OPTION, "Failed to set socket option"},
    {ERR_HOST_NOT_FOUND, "Host not found"},
    {ERR_CONNECTION_REFUSED, "Connection refused"},
    {ERR_CONNECTION_RESET, "Connection reset by peer"},
    {ERR_CONNECTION_TIMEOUT, "Connection timed out"},
    {ERR_NETWORK_UNREACHABLE, "Network is unreachable"},
    {ERR_ADDRESS_IN_USE, "Address already in use"},

    /* USB Errors */
    {ERR_USB_INIT, "USB initialization failed"},
    {ERR_USB_NOT_FOUND, "USB device not found"},
    {ERR_USB_ACCESS_DENIED, "USB device access denied"},
    {ERR_USB_BUSY, "USB device is busy"},
    {ERR_USB_NO_DEVICE, "No USB device"},
    {ERR_USB_CLAIM_FAILED, "Failed to claim USB interface"},
    {ERR_USB_RELEASE_FAILED, "Failed to release USB interface"},
    {ERR_USB_IO_FAILED, "USB I/O operation failed"},
    {ERR_USB_STALL, "USB endpoint stall"},
    {ERR_USB_OVERFLOW, "USB data overflow"},
    {ERR_USB_PIPE_ERROR, "USB pipe error"},
    {ERR_USB_INTERRUPTED, "USB operation interrupted"},
    {ERR_USB_NO_MEMORY, "USB no memory"},
    {ERR_USB_INVALID_ENDPOINT, "Invalid USB endpoint"},
    {ERR_USB_INVALID_INTERFACE, "Invalid USB interface"},
    {ERR_USB_DETACHED, "USB device detached"},
    {ERR_USB_DESCRIPTOR_ERROR, "USB descriptor error"},

    /* Protocol Errors */
    {ERR_PROTOCOL_VERSION, "Protocol version mismatch"},
    {ERR_PROTOCOL_INVALID, "Invalid protocol data"},
    {ERR_PROTOCOL_UNSUPPORTED, "Unsupported protocol operation"},
    {ERR_PROTOCOL_SEQUENCE, "Protocol sequence error"},
    {ERR_PROTOCOL_HEADER, "Invalid protocol header"},
    {ERR_PROTOCOL_DATA, "Invalid protocol data"},
    {ERR_PROTOCOL_CHECKSUM, "Protocol checksum error"},
    {ERR_PROTOCOL_STATE, "Invalid protocol state"},

    /* VHCI Errors */
    {ERR_VHCI_INIT, "VHCI initialization failed"},
    {ERR_VHCI_NOT_FOUND, "VHCI driver not found"},
    {ERR_VHCI_ATTACH_FAILED, "Failed to attach device to VHCI"},
    {ERR_VHCI_DETACH_FAILED, "Failed to detach device from VHCI"},
    {ERR_VHCI_NO_FREE_PORT, "No free VHCI port available"},
    {ERR_VHCI_INVALID_PORT, "Invalid VHCI port"},
    {ERR_VHCI_DRIVER_ERROR, "VHCI driver error"},

    /* System Errors */
    {ERR_SYSTEM_INIT, "System initialization failed"},
    {ERR_THREAD_CREATE, "Failed to create thread"},
    {ERR_THREAD_JOIN, "Failed to join thread"},
    {ERR_MUTEX_INIT, "Failed to initialize mutex"},
    {ERR_MUTEX_LOCK, "Failed to lock mutex"},
    {ERR_FILE_OPEN, "Failed to open file"},
    {ERR_FILE_READ, "Failed to read file"},
    {ERR_FILE_WRITE, "Failed to write file"},
    {ERR_REGISTRY_ACCESS, "Failed to access registry"},

    /* End marker */
    {ERR_MAX, NULL}
};

/*
 * Get error string from error code
 */
const char* error_string(error_code_t code) {
    for (int i = 0; error_messages[i].message != NULL; i++) {
        if (error_messages[i].code == code) {
            return error_messages[i].message;
        }
    }
    return "Unknown error";
}

/*
 * Get detailed error message including system error
 */
const char* error_detail(void) {
    char sys_error_msg[256] = {0};

    /* Get Windows error message if available */
    if (g_last_error.system_error != 0) {
        FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            g_last_error.system_error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            sys_error_msg,
            sizeof(sys_error_msg),
            NULL
        );
        /* Remove trailing newline */
        size_t len = strlen(sys_error_msg);
        while (len > 0 && (sys_error_msg[len-1] == '\n' || sys_error_msg[len-1] == '\r')) {
            sys_error_msg[--len] = '\0';
        }
    }

    /* Build detailed message */
    if (g_last_error.message[0] != '\0') {
        if (sys_error_msg[0] != '\0') {
            snprintf(g_error_detail_buffer, sizeof(g_error_detail_buffer),
                "%s: %s (System: %s [%lu])",
                error_string(g_last_error.code),
                g_last_error.message,
                sys_error_msg,
                g_last_error.system_error);
        } else {
            snprintf(g_error_detail_buffer, sizeof(g_error_detail_buffer),
                "%s: %s",
                error_string(g_last_error.code),
                g_last_error.message);
        }
    } else if (sys_error_msg[0] != '\0') {
        snprintf(g_error_detail_buffer, sizeof(g_error_detail_buffer),
            "%s (System: %s [%lu])",
            error_string(g_last_error.code),
            sys_error_msg,
            g_last_error.system_error);
    } else {
        snprintf(g_error_detail_buffer, sizeof(g_error_detail_buffer),
            "%s",
            error_string(g_last_error.code));
    }

    return g_error_detail_buffer;
}

/*
 * Set error context
 */
void error_set(error_code_t code, const char *file, int line, const char *func, const char *fmt, ...) {
    g_last_error.code = code;
    g_last_error.system_error = GetLastError();
    g_last_error.socket_error = WSAGetLastError();
    g_last_error.file = file;
    g_last_error.line = line;
    g_last_error.function = func;

    if (fmt != NULL) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_last_error.message, sizeof(g_last_error.message), fmt, args);
        va_end(args);
    } else {
        g_last_error.message[0] = '\0';
    }
}

/*
 * Clear error context
 */
void error_clear(void) {
    memset(&g_last_error, 0, sizeof(g_last_error));
    SetLastError(0);
    WSASetLastError(0);
}

/*
 * Get last error code
 */
error_code_t error_get_code(void) {
    return g_last_error.code;
}

/*
 * Get last system error code
 */
DWORD error_get_system(void) {
    return g_last_error.system_error;
}
