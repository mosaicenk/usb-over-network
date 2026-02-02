/*
 * USB Over Network - Device List Management
 * Windows-only implementation
 *
 * Thread-safe management of shared USB devices
 */

#ifndef DEVICE_LIST_H
#define DEVICE_LIST_H

#include "../common/types.h"
#include "../common/error.h"
#include "usb_host.h"

/* Device states */
typedef enum device_state {
    DEVICE_STATE_AVAILABLE = 0,     /* Device is available for sharing */
    DEVICE_STATE_EXPORTED,          /* Device is exported to a client */
    DEVICE_STATE_BUSY,              /* Device is temporarily busy */
    DEVICE_STATE_ERROR              /* Device is in error state */
} device_state_t;

/* Device entry in the list */
typedef struct device_entry {
    usb_device_t device;            /* USB device information */
    device_state_t state;           /* Current state */
    uint32_t client_id;             /* ID of client using this device (if exported) */
    char client_ip[64];             /* IP address of client (if exported) */
    uint64_t export_time;           /* Timestamp when device was exported */
    bool is_shared;                 /* Whether this device is available for sharing */
    struct device_entry *next;      /* Next entry in list */
} device_entry_t;

/* Device list structure */
typedef struct device_list {
    device_entry_t *head;           /* Head of linked list */
    int count;                      /* Number of devices */
    mutex_t mutex;                  /* Thread safety mutex */
    bool initialized;               /* Initialization flag */
} device_list_t;

/* ----- Initialization ----- */

/* Initialize device list */
error_code_t device_list_init(device_list_t *list);

/* Cleanup device list */
void device_list_cleanup(device_list_t *list);

/* ----- Device Management ----- */

/* Refresh device list (rescan USB devices) */
error_code_t device_list_refresh(device_list_t *list);

/* Add device to list */
error_code_t device_list_add(device_list_t *list, const usb_device_t *device);

/* Remove device from list by bus ID */
error_code_t device_list_remove(device_list_t *list, const char *busid);

/* Remove device from list by entry */
error_code_t device_list_remove_entry(device_list_t *list, device_entry_t *entry);

/* Clear all devices from list */
void device_list_clear(device_list_t *list);

/* ----- Device Lookup ----- */

/* Find device by bus ID */
device_entry_t* device_list_find(device_list_t *list, const char *busid);

/* Find device by VID/PID */
device_entry_t* device_list_find_by_vidpid(device_list_t *list, uint16_t vid, uint16_t pid);

/* Find device by device path */
device_entry_t* device_list_find_by_path(device_list_t *list, const char *path);

/* Get device count */
int device_list_count(device_list_t *list);

/* Get available device count */
int device_list_available_count(device_list_t *list);

/* ----- Device State ----- */

/* Mark device as exported */
error_code_t device_list_export(device_list_t *list, const char *busid,
                                 uint32_t client_id, const char *client_ip);

/* Mark device as available (release export) */
error_code_t device_list_unexport(device_list_t *list, const char *busid);

/* Mark device as busy */
error_code_t device_list_set_busy(device_list_t *list, const char *busid, bool busy);

/* Check if device is available */
bool device_list_is_available(device_list_t *list, const char *busid);

/* Check if device is exported */
bool device_list_is_exported(device_list_t *list, const char *busid);

/* Get device state */
device_state_t device_list_get_state(device_list_t *list, const char *busid);

/* ----- Filtering ----- */

/* Set device sharing flag */
error_code_t device_list_set_shared(device_list_t *list, const char *busid, bool shared);

/* Share all devices */
void device_list_share_all(device_list_t *list);

/* Unshare all devices */
void device_list_unshare_all(device_list_t *list);

/* ----- Iteration ----- */

/* Callback for device iteration */
typedef bool (*device_list_callback_t)(device_entry_t *entry, void *user_data);

/* Iterate over all devices */
void device_list_foreach(device_list_t *list, device_list_callback_t callback, void *user_data);

/* Iterate over available devices */
void device_list_foreach_available(device_list_t *list, device_list_callback_t callback, void *user_data);

/* Iterate over shared devices */
void device_list_foreach_shared(device_list_t *list, device_list_callback_t callback, void *user_data);

/* ----- Locking ----- */

/* Lock device list for exclusive access */
void device_list_lock(device_list_t *list);

/* Unlock device list */
void device_list_unlock(device_list_t *list);

/* ----- Utility ----- */

/* Print device list for debugging */
void device_list_print(device_list_t *list);

/* Get device state string */
const char* device_state_string(device_state_t state);

#endif /* DEVICE_LIST_H */
