/*
 * USB Over Network - Type Definitions
 * Windows-only implementation
 *
 * Platform-independent type definitions for USB/IP implementation
 */

#ifndef TYPES_H
#define TYPES_H

/* Windows headers */
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Link with Winsock library (MSVC only) */
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#endif

/* Socket type abstraction */
typedef SOCKET socket_t;
#define INVALID_SOCKET_VAL INVALID_SOCKET
#define SOCKET_ERROR_VAL SOCKET_ERROR

/* Close socket macro */
#define close_socket(s) closesocket(s)

/* Thread type abstraction */
typedef HANDLE thread_t;
typedef HANDLE mutex_t;
typedef DWORD thread_ret_t;
#define THREAD_CALL __stdcall

/* Thread functions */
#define thread_create(t, func, arg) \
    ((*(t) = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(func), (arg), 0, NULL)) != NULL ? 0 : -1)
#define thread_join(t) WaitForSingleObject((t), INFINITE)
#define thread_detach(t) CloseHandle(t)

/* Mutex functions */
#define mutex_init(m) ((*(m) = CreateMutex(NULL, FALSE, NULL)) != NULL ? 0 : -1)
#define mutex_destroy(m) CloseHandle(*(m))
#define mutex_lock(m) WaitForSingleObject(*(m), INFINITE)
#define mutex_unlock(m) ReleaseMutex(*(m))

/* ssize_t for Windows */
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
#ifdef _WIN64
typedef __int64 ssize_t;
#else
typedef int ssize_t;
#endif
#endif

/* Sleep function */
#define sleep_ms(ms) Sleep(ms)
#define sleep_sec(s) Sleep((s) * 1000)

/* Auto-reset event: used to wake worker threads without busy-polling.
 * Use event_wait/event_wait_timeout inside a check loop. */
typedef HANDLE event_t;
#define event_init(e)       ((*(e) = CreateEvent(NULL, FALSE, FALSE, NULL)) != NULL ? 0 : -1)
#define event_destroy(e)    CloseHandle(*(e))
#define event_signal(e)     (SetEvent(*(e)) ? 0 : -1)
#define event_wait(e)       WaitForSingleObject(*(e), INFINITE)
#define event_wait_timeout(e, ms) WaitForSingleObject(*(e), (ms))

/* Packed structure attribute */
#ifdef _MSC_VER
    #define PACKED_BEGIN __pragma(pack(push, 1))
    #define PACKED_END __pragma(pack(pop))
    #define PACKED_ATTR
#else
    /* GCC/MinGW - use _Pragma for C99 compatibility */
    #define PACKED_BEGIN _Pragma("pack(push, 1)")
    #define PACKED_END _Pragma("pack(pop)")
    #define PACKED_ATTR
#endif

/* Inline attribute */
#ifdef _MSC_VER
    #define INLINE __inline
#else
    #define INLINE inline
#endif

/* Format specifiers for 64-bit integers */
#ifdef _MSC_VER
    #define PRIu64 "I64u"
    #define PRId64 "I64d"
    #define PRIx64 "I64x"
#else
    #include <inttypes.h>
    /* PRIu64, PRId64, PRIx64 are defined in inttypes.h for GCC */
#endif

/* Error number */
#define get_last_error() GetLastError()
#define get_socket_error() WSAGetLastError()

/* Path separator */
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_STR "\\"

/* Maximum path length */
#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN MAX_PATH
#endif

/* Atomic operations */
#define atomic_inc(v) InterlockedIncrement((volatile LONG*)(v))
#define atomic_dec(v) InterlockedDecrement((volatile LONG*)(v))
#define atomic_add(v, n) InterlockedAdd((volatile LONG*)(v), (n))

/* Byte order conversion */
#define htobe16(x) htons(x)
#define htobe32(x) htonl(x)
#define be16toh(x) ntohs(x)
#define be32toh(x) ntohl(x)

/* 64-bit byte order conversion */
static INLINE uint64_t htobe64(uint64_t x) {
    return ((uint64_t)htonl((uint32_t)(x & 0xFFFFFFFF)) << 32) |
           (uint64_t)htonl((uint32_t)(x >> 32));
}

static INLINE uint64_t be64toh(uint64_t x) {
    return ((uint64_t)ntohl((uint32_t)(x & 0xFFFFFFFF)) << 32) |
           (uint64_t)ntohl((uint32_t)(x >> 32));
}

/* Boolean type */
#ifndef __bool_true_false_are_defined
#define __bool_true_false_are_defined 1
#endif

/* Result codes */
typedef int result_t;
#define RESULT_OK       0
#define RESULT_ERROR   -1

/* Buffer size type */
typedef size_t bufsize_t;

/* Device ID type */
typedef uint32_t devid_t;

/* Sequence number type */
typedef uint32_t seqnum_t;

#endif /* TYPES_H */
