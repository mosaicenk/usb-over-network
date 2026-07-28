/*
 * USB Over Network - Device List Implementation
 * Windows-only implementation
 */

#include "device_list.h"
#include "../common/log.h"
#include "../common/string_utils.h"
#include <string.h>
#include <time.h>

/* ----- Helper Functions ----- */

static device_entry_t* create_entry(const usb_device_t *device) {
    device_entry_t *entry = (device_entry_t *)calloc(1, sizeof(device_entry_t));
    if (entry == NULL) {
        return NULL;
    }

    memcpy(&entry->device, device, sizeof(usb_device_t));
    entry->state = DEVICE_STATE_AVAILABLE;
    entry->is_shared = true;  /* Share by default */
    entry->next = NULL;

    return entry;
}

static void free_entry(device_entry_t *entry) {
    if (entry != NULL) {
        /* Close device if open */
        if (entry->device.is_open) {
            usb_close_device(&entry->device);
        }
        free(entry);
    }
}

static uint64_t get_timestamp(void) {
    return (uint64_t)time(NULL);
}

/* ----- Initialization ----- */

error_code_t device_list_init(device_list_t *list) {
    if (list == NULL) {
        return ERR_INVALID_PARAM;
    }

    memset(list, 0, sizeof(device_list_t));

    if (mutex_init(&list->mutex) != 0) {
        LOG_ERROR("Failed to initialize device list mutex");
        return ERR_MUTEX_INIT;
    }

    list->initialized = true;
    LOG_DEBUG("Device list initialized");

    return ERR_SUCCESS;
}

void device_list_cleanup(device_list_t *list) {
    if (list == NULL || !list->initialized) {
        return;
    }

    device_list_lock(list);
    device_list_clear(list);
    list->initialized = false;
    device_list_unlock(list);

    mutex_destroy(&list->mutex);
    LOG_DEBUG("Device list cleaned up");
}

/* ----- Device Management ----- */

/* Callback for enumeration */
static void enum_callback(const usb_device_t *device, void *user_data) {
    device_list_t *list = (device_list_t *)user_data;
    device_list_add(list, device);
}

error_code_t device_list_refresh(device_list_t *list) {
    if (list == NULL || !list->initialized) {
        return ERR_INVALID_PARAM;
    }

    LOG_DEBUG("Refreshing device list");

    device_list_lock(list);

    /* Remember exported devices */
    typedef struct {
        char busid[USBIP_BUSID_MAX];
        uint32_t client_id;
        char client_ip[64];
    } export_info_t;

    export_info_t exports[MAX_DEVICES];
    int export_count = 0;

    /* Save export info */
    device_entry_t *entry = list->head;
    while (entry != NULL && export_count < MAX_DEVICES) {
        if (entry->state == DEVICE_STATE_EXPORTED) {
            str_copy(exports[export_count].busid, entry->device.busid, USBIP_BUSID_MAX);
            exports[export_count].client_id = entry->client_id;
            str_copy(exports[export_count].client_ip, entry->client_ip, 64);
            export_count++;
        }
        entry = entry->next;
    }

    /* Clear current list */
    device_list_clear(list);

    /* Re-enumerate devices */
    usb_enumerate_devices(enum_callback, list);

    /* Restore export info */
    for (int i = 0; i < export_count; i++) {
        entry = device_list_find(list, exports[i].busid);
        if (entry != NULL) {
            entry->state = DEVICE_STATE_EXPORTED;
            entry->client_id = exports[i].client_id;
            str_copy(entry->client_ip, exports[i].client_ip, 64);
            entry->export_time = get_timestamp();
        }
    }

    device_list_unlock(list);

    LOG_INFO("Device list refreshed: %d devices", list->count);
    return ERR_SUCCESS;
}

error_code_t device_list_add(device_list_t *list, const usb_device_t *device) {
    if (list == NULL || device == NULL) {
        return ERR_INVALID_PARAM;
    }

    /* Check if device already exists */
    device_entry_t *existing = device_list_find(list, device->busid);
    if (existing != NULL) {
        /* Update device info */
        memcpy(&existing->device, device, sizeof(usb_device_t));
        return ERR_SUCCESS;
    }

    /* Create new entry */
    device_entry_t *entry = create_entry(device);
    if (entry == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    /* Add to list (already locked by caller or needs locking) */
    entry->next = list->head;
    list->head = entry;
    list->count++;

    LOG_DEBUG("Added device: %s (VID=%04X PID=%04X)",
        device->busid, device->vendor_id, device->product_id);

    return ERR_SUCCESS;
}

error_code_t device_list_remove(device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    device_list_lock(list);

    device_entry_t *prev = NULL;
    device_entry_t *entry = list->head;

    while (entry != NULL) {
        if (strcmp(entry->device.busid, busid) == 0) {
            /* Unlink from list */
            if (prev == NULL) {
                list->head = entry->next;
            } else {
                prev->next = entry->next;
            }
            list->count--;

            LOG_DEBUG("Removed device: %s", busid);

            free_entry(entry);
            device_list_unlock(list);
            return ERR_SUCCESS;
        }
        prev = entry;
        entry = entry->next;
    }

    device_list_unlock(list);
    return ERR_NOT_FOUND;
}

error_code_t device_list_remove_entry(device_list_t *list, device_entry_t *target) {
    if (list == NULL || target == NULL) {
        return ERR_INVALID_PARAM;
    }

    return device_list_remove(list, target->device.busid);
}

void device_list_clear(device_list_t *list) {
    if (list == NULL) {
        return;
    }

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        device_entry_t *next = entry->next;
        free_entry(entry);
        entry = next;
    }

    list->head = NULL;
    list->count = 0;
}

/* ----- Device Lookup ----- */

device_entry_t* device_list_find(device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL) {
        return NULL;
    }

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        if (strcmp(entry->device.busid, busid) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

device_entry_t* device_list_find_by_vidpid(device_list_t *list, uint16_t vid, uint16_t pid) {
    if (list == NULL) {
        return NULL;
    }

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        if (entry->device.vendor_id == vid && entry->device.product_id == pid) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

device_entry_t* device_list_find_by_path(device_list_t *list, const char *path) {
    if (list == NULL || path == NULL) {
        return NULL;
    }

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        if (strcmp(entry->device.path, path) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

int device_list_count(device_list_t *list) {
    return (list != NULL) ? list->count : 0;
}

int device_list_available_count(device_list_t *list) {
    if (list == NULL) {
        return 0;
    }

    int count = 0;
    device_entry_t *entry = list->head;
    while (entry != NULL) {
        if (entry->state == DEVICE_STATE_AVAILABLE && entry->is_shared) {
            count++;
        }
        entry = entry->next;
    }

    return count;
}

/* ----- Device State ----- */

error_code_t device_list_export(device_list_t *list, const char *busid,
                                 uint32_t client_id, const char *client_ip) {
    if (list == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    device_list_lock(list);

    device_entry_t *entry = device_list_find(list, busid);
    if (entry == NULL) {
        device_list_unlock(list);
        return ERR_NOT_FOUND;
    }

    if (entry->state != DEVICE_STATE_AVAILABLE) {
        device_list_unlock(list);
        return ERR_DEVICE_BUSY;
    }

    entry->state = DEVICE_STATE_EXPORTED;
    entry->client_id = client_id;
    if (client_ip != NULL) {
        str_copy(entry->client_ip, client_ip, sizeof(entry->client_ip));
    }
    entry->export_time = get_timestamp();

    LOG_INFO("Device %s exported to client %u (%s)", busid, client_id,
        client_ip ? client_ip : "unknown");

    device_list_unlock(list);
    return ERR_SUCCESS;
}

error_code_t device_list_unexport(device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    device_list_lock(list);

    device_entry_t *entry = device_list_find(list, busid);
    if (entry == NULL) {
        device_list_unlock(list);
        return ERR_NOT_FOUND;
    }

    entry->state = DEVICE_STATE_AVAILABLE;
    entry->client_id = 0;
    entry->client_ip[0] = '\0';
    entry->export_time = 0;

    LOG_INFO("Device %s unexported", busid);

    device_list_unlock(list);
    return ERR_SUCCESS;
}

error_code_t device_list_set_busy(device_list_t *list, const char *busid, bool busy) {
    if (list == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    device_list_lock(list);

    device_entry_t *entry = device_list_find(list, busid);
    if (entry == NULL) {
        device_list_unlock(list);
        return ERR_NOT_FOUND;
    }

    if (busy && entry->state == DEVICE_STATE_AVAILABLE) {
        entry->state = DEVICE_STATE_BUSY;
    } else if (!busy && entry->state == DEVICE_STATE_BUSY) {
        entry->state = DEVICE_STATE_AVAILABLE;
    }

    device_list_unlock(list);
    return ERR_SUCCESS;
}

bool device_list_is_available(device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL) {
        return false;
    }

    device_list_lock(list);
    device_entry_t *entry = device_list_find(list, busid);
    bool available = (entry != NULL && entry->state == DEVICE_STATE_AVAILABLE && entry->is_shared);
    device_list_unlock(list);

    return available;
}

bool device_list_is_exported(device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL) {
        return false;
    }

    device_list_lock(list);
    device_entry_t *entry = device_list_find(list, busid);
    bool exported = (entry != NULL && entry->state == DEVICE_STATE_EXPORTED);
    device_list_unlock(list);

    return exported;
}

device_state_t device_list_get_state(device_list_t *list, const char *busid) {
    if (list == NULL || busid == NULL) {
        return DEVICE_STATE_ERROR;
    }

    device_list_lock(list);
    device_entry_t *entry = device_list_find(list, busid);
    device_state_t state = (entry != NULL) ? entry->state : DEVICE_STATE_ERROR;
    device_list_unlock(list);

    return state;
}

/* ----- Filtering ----- */

error_code_t device_list_set_shared(device_list_t *list, const char *busid, bool shared) {
    if (list == NULL || busid == NULL) {
        return ERR_INVALID_PARAM;
    }

    device_list_lock(list);

    device_entry_t *entry = device_list_find(list, busid);
    if (entry == NULL) {
        device_list_unlock(list);
        return ERR_NOT_FOUND;
    }

    entry->is_shared = shared;
    LOG_DEBUG("Device %s sharing: %s", busid, shared ? "enabled" : "disabled");

    device_list_unlock(list);
    return ERR_SUCCESS;
}

void device_list_share_all(device_list_t *list) {
    if (list == NULL) {
        return;
    }

    device_list_lock(list);

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        entry->is_shared = true;
        entry = entry->next;
    }

    device_list_unlock(list);
    LOG_DEBUG("All devices set to shared");
}

void device_list_unshare_all(device_list_t *list) {
    if (list == NULL) {
        return;
    }

    device_list_lock(list);

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        entry->is_shared = false;
        entry = entry->next;
    }

    device_list_unlock(list);
    LOG_DEBUG("All devices set to not shared");
}

/* ----- Iteration ----- */

void device_list_foreach(device_list_t *list, device_list_callback_t callback, void *user_data) {
    if (list == NULL || callback == NULL) {
        return;
    }

    device_list_lock(list);

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        device_entry_t *next = entry->next;
        if (!callback(entry, user_data)) {
            break;
        }
        entry = next;
    }

    device_list_unlock(list);
}

void device_list_foreach_available(device_list_t *list, device_list_callback_t callback, void *user_data) {
    if (list == NULL || callback == NULL) {
        return;
    }

    device_list_lock(list);

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        device_entry_t *next = entry->next;
        if (entry->state == DEVICE_STATE_AVAILABLE) {
            if (!callback(entry, user_data)) {
                break;
            }
        }
        entry = next;
    }

    device_list_unlock(list);
}

void device_list_foreach_shared(device_list_t *list, device_list_callback_t callback, void *user_data) {
    if (list == NULL || callback == NULL) {
        return;
    }

    device_list_lock(list);

    device_entry_t *entry = list->head;
    while (entry != NULL) {
        device_entry_t *next = entry->next;
        if (entry->is_shared) {
            if (!callback(entry, user_data)) {
                break;
            }
        }
        entry = next;
    }

    device_list_unlock(list);
}

/* ----- Locking ----- */

void device_list_lock(device_list_t *list) {
    if (list != NULL && list->initialized) {
        mutex_lock(&list->mutex);
    }
}

void device_list_unlock(device_list_t *list) {
    if (list != NULL && list->initialized) {
        mutex_unlock(&list->mutex);
    }
}

/* ----- Utility ----- */

void device_list_print(device_list_t *list) {
    if (list == NULL) {
        return;
    }

    device_list_lock(list);

    LOG_INFO("Device List (%d devices):", list->count);
    LOG_INFO("----------------------------------------");

    device_entry_t *entry = list->head;
    int index = 0;
    while (entry != NULL) {
        LOG_INFO("[%d] %s - %04X:%04X (%s)",
            index++,
            entry->device.busid,
            entry->device.vendor_id,
            entry->device.product_id,
            device_state_string(entry->state));
        LOG_INFO("    Product: %s", entry->device.product);
        LOG_INFO("    Shared: %s", entry->is_shared ? "Yes" : "No");
        if (entry->state == DEVICE_STATE_EXPORTED) {
            LOG_INFO("    Client: %u (%s)", entry->client_id, entry->client_ip);
        }
        entry = entry->next;
    }

    LOG_INFO("----------------------------------------");

    device_list_unlock(list);
}

const char* device_state_string(device_state_t state) {
    switch (state) {
        case DEVICE_STATE_AVAILABLE: return "Available";
        case DEVICE_STATE_EXPORTED:  return "Exported";
        case DEVICE_STATE_BUSY:      return "Busy";
        case DEVICE_STATE_ERROR:     return "Error";
        default:                     return "Unknown";
    }
}
