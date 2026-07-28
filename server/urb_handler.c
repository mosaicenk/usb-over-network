/*
 * USB Over Network - URB Handler Implementation
 * Windows-only implementation
 */

#include "urb_handler.h"
#include "../common/log.h"
#include "../common/config.h"
#include <time.h>

/* ----- Helper Functions ----- */

static uint64_t get_timestamp_ms(void) {
    return GetTickCount64();
}

static urb_entry_t* create_urb_entry(void) {
    urb_entry_t *entry = (urb_entry_t *)calloc(1, sizeof(urb_entry_t));
    return entry;
}

static void free_urb_entry(urb_entry_t *entry) {
    if (entry != NULL) {
        if (entry->urb.buffer != NULL) {
            free(entry->urb.buffer);
        }
        free(entry);
    }
}

static error_code_t queue_init(urb_queue_t *queue) {
    memset(queue, 0, sizeof(urb_queue_t));
    if (mutex_init(&queue->mutex) != 0) {
        return ERR_MUTEX_INIT;
    }
    if (event_init(&queue->notify) != 0) {
        mutex_destroy(&queue->mutex);
        return ERR_THREAD_CREATE;
    }
    return ERR_SUCCESS;
}

static void queue_cleanup(urb_queue_t *queue) {
    mutex_lock(&queue->mutex);

    urb_entry_t *entry = queue->head;
    while (entry != NULL) {
        urb_entry_t *next = entry->next;
        free_urb_entry(entry);
        entry = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    mutex_unlock(&queue->mutex);
    event_destroy(&queue->notify);
    mutex_destroy(&queue->mutex);
}

static void queue_push(urb_queue_t *queue, urb_entry_t *entry) {
    mutex_lock(&queue->mutex);

    entry->next = NULL;
    if (queue->tail != NULL) {
        queue->tail->next = entry;
    } else {
        queue->head = entry;
    }
    queue->tail = entry;
    queue->count++;

    mutex_unlock(&queue->mutex);
    /* Wake one waiter; auto-reset event drops the signal if none is parked. */
    event_signal(&queue->notify);
}

static urb_entry_t* queue_pop(urb_queue_t *queue) {
    mutex_lock(&queue->mutex);

    urb_entry_t *entry = queue->head;
    if (entry != NULL) {
        queue->head = entry->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        queue->count--;
        entry->next = NULL;
    }

    mutex_unlock(&queue->mutex);
    return entry;
}

static urb_entry_t* queue_find_remove(urb_queue_t *queue, uint32_t seqnum) {
    mutex_lock(&queue->mutex);

    urb_entry_t *prev = NULL;
    urb_entry_t *entry = queue->head;

    while (entry != NULL) {
        if (entry->seqnum == seqnum) {
            if (prev == NULL) {
                queue->head = entry->next;
            } else {
                prev->next = entry->next;
            }
            if (entry == queue->tail) {
                queue->tail = prev;
            }
            queue->count--;
            entry->next = NULL;
            mutex_unlock(&queue->mutex);
            return entry;
        }
        prev = entry;
        entry = entry->next;
    }

    mutex_unlock(&queue->mutex);
    return NULL;
}

/* ----- Worker Thread ----- */

static DWORD WINAPI urb_worker_thread(LPVOID param) {
    urb_handler_t *handler = (urb_handler_t *)param;

    LOG_DEBUG("URB worker thread started");

    while (handler->running) {
        /* Try a non-blocking pop first; common case skips the wait. */
        urb_entry_t *entry = queue_pop(&handler->pending);

        if (entry == NULL) {
            /* Park on the auto-reset event. A 100ms watchdog lets us observe
             * handler->running going false even if a signal is missed; this
             * avoids the previous busy-loop (Sleep(1) consumed a full core). */
            if (event_wait_timeout(&handler->pending.notify, 100) == WAIT_FAILED) {
                break;
            }
            continue;
        }

        LOG_DEBUG("Processing URB: seq=%u ep=%u dir=%u len=%u",
            entry->seqnum, entry->urb.endpoint, entry->urb.direction,
            entry->urb.buffer_length);

        entry->in_progress = true;

        /* Execute URB on USB device */
        error_code_t result = usb_submit_urb(handler->device, &entry->urb);

        entry->completed = true;
        entry->in_progress = false;

        if (result != ERR_SUCCESS) {
            LOG_WARN("URB failed: seq=%u err=%d", entry->seqnum, result);
            entry->urb.status = -EIO;
        }

        /* Move to completed queue */
        queue_push(&handler->completed, entry);
    }

    LOG_DEBUG("URB worker thread stopped");
    return 0;
}

/* ----- Initialization ----- */

error_code_t urb_handler_init(urb_handler_t *handler, usb_device_t *device) {
    if (handler == NULL || device == NULL) {
        return ERR_INVALID_PARAM;
    }

    memset(handler, 0, sizeof(urb_handler_t));
    handler->device = device;
    handler->next_seqnum = 1;

    error_code_t err = queue_init(&handler->pending);
    if (err != ERR_SUCCESS) {
        return err;
    }

    err = queue_init(&handler->completed);
    if (err != ERR_SUCCESS) {
        queue_cleanup(&handler->pending);
        return err;
    }

    handler->initialized = true;
    LOG_DEBUG("URB handler initialized for device %s", device->busid);

    return ERR_SUCCESS;
}

void urb_handler_cleanup(urb_handler_t *handler) {
    if (handler == NULL || !handler->initialized) {
        return;
    }

    urb_handler_stop(handler);

    queue_cleanup(&handler->pending);
    queue_cleanup(&handler->completed);

    handler->initialized = false;
    LOG_DEBUG("URB handler cleaned up");
}

/* ----- URB Processing ----- */

error_code_t urb_handler_submit(urb_handler_t *handler,
                                 const usbip_header_t *header,
                                 const uint8_t *data, uint32_t data_len) {
    if (handler == NULL || header == NULL || !handler->initialized) {
        return ERR_INVALID_PARAM;
    }

    urb_entry_t *entry = create_urb_entry();
    if (entry == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    /* Copy header info */
    entry->seqnum = header->common.seqnum;
    entry->devid = header->common.devid;
    entry->submit_time = get_timestamp_ms();

    /* Setup URB */
    entry->urb.seqnum = header->common.seqnum;
    entry->urb.devid = header->common.devid;
    entry->urb.endpoint = (uint8_t)header->common.ep;
    entry->urb.direction = (uint8_t)header->common.direction;
    entry->urb.transfer_flags = header->u.cmd_submit.transfer_flags;
    entry->urb.buffer_length = header->u.cmd_submit.transfer_buffer_length;
    entry->urb.interval = header->u.cmd_submit.interval;
    entry->urb.start_frame = header->u.cmd_submit.start_frame;
    entry->urb.number_of_packets = header->u.cmd_submit.number_of_packets;

    /* Copy setup packet for control transfers */
    memcpy(entry->urb.setup, header->u.cmd_submit.setup, 8);

    /* Determine transfer type from endpoint */
    if (entry->urb.endpoint == 0) {
        entry->urb.type = USB_ENDPOINT_XFER_CONTROL;
    } else {
        entry->urb.type = USB_ENDPOINT_XFER_BULK;  /* Default to bulk */
    }

    /* Set direction in endpoint address */
    if (entry->urb.direction == USBIP_DIR_IN) {
        entry->urb.endpoint |= USB_DIR_IN;
    }

    /* Allocate data buffer */
    if (entry->urb.buffer_length > 0) {
        entry->urb.buffer = (uint8_t *)malloc(entry->urb.buffer_length);
        if (entry->urb.buffer == NULL) {
            free_urb_entry(entry);
            return ERR_OUT_OF_MEMORY;
        }

        /* Copy OUT data */
        if (entry->urb.direction == USBIP_DIR_OUT && data != NULL && data_len > 0) {
            memcpy(entry->urb.buffer, data, (data_len < entry->urb.buffer_length) ?
                data_len : entry->urb.buffer_length);
        }
    }

    LOG_DEBUG("URB submit: seq=%u ep=%02X dir=%s len=%u",
        entry->seqnum, entry->urb.endpoint,
        entry->urb.direction == USBIP_DIR_IN ? "IN" : "OUT",
        entry->urb.buffer_length);

    /* Add to pending queue */
    queue_push(&handler->pending, entry);

    return ERR_SUCCESS;
}

error_code_t urb_handler_unlink(urb_handler_t *handler, uint32_t seqnum_to_unlink) {
    if (handler == NULL || !handler->initialized) {
        return ERR_INVALID_PARAM;
    }

    LOG_DEBUG("URB unlink request: seq=%u", seqnum_to_unlink);

    /* Try to find in pending queue */
    urb_entry_t *entry = queue_find_remove(&handler->pending, seqnum_to_unlink);

    if (entry != NULL) {
        /* URB found and removed from pending */
        entry->urb.status = -ECANCELED;
        entry->urb.cancelled = true;
        entry->completed = true;

        /* Move to completed queue */
        queue_push(&handler->completed, entry);
        LOG_DEBUG("URB unlinked: seq=%u", seqnum_to_unlink);
        return ERR_SUCCESS;
    }

    /* URB may be in progress - can't cancel */
    LOG_WARN("URB not found for unlink: seq=%u", seqnum_to_unlink);
    return ERR_NOT_FOUND;
}

urb_entry_t* urb_handler_get_completed(urb_handler_t *handler) {
    if (handler == NULL || !handler->initialized) {
        return NULL;
    }

    return queue_pop(&handler->completed);
}

void urb_handler_free_entry(urb_entry_t *entry) {
    free_urb_entry(entry);
}

/* ----- Control ----- */

error_code_t urb_handler_start(urb_handler_t *handler) {
    if (handler == NULL || !handler->initialized) {
        return ERR_INVALID_PARAM;
    }

    if (handler->running) {
        return ERR_SUCCESS;
    }

    handler->running = true;

    if (thread_create(&handler->worker_thread, urb_worker_thread, handler) != 0) {
        handler->running = false;
        LOG_ERROR("Failed to create URB worker thread");
        return ERR_THREAD_CREATE;
    }

    LOG_INFO("URB handler started");
    return ERR_SUCCESS;
}

void urb_handler_stop(urb_handler_t *handler) {
    if (handler == NULL || !handler->running) {
        return;
    }

    handler->running = false;
    /* Nudge the worker off its event wait so it observes the flag and exits. */
    event_signal(&handler->pending.notify);

    /* Wait for worker thread to finish */
    thread_join(handler->worker_thread);
    CloseHandle(handler->worker_thread);

    /* Cancel any remaining pending URBs */
    urb_handler_cancel_all(handler);

    LOG_INFO("URB handler stopped");
}

void urb_handler_cancel_all(urb_handler_t *handler) {
    if (handler == NULL) {
        return;
    }

    urb_entry_t *entry;
    while ((entry = queue_pop(&handler->pending)) != NULL) {
        entry->urb.status = -ECANCELED;
        entry->urb.cancelled = true;
        entry->completed = true;
        queue_push(&handler->completed, entry);
    }
}

/* ----- Status ----- */

int urb_handler_pending_count(urb_handler_t *handler) {
    if (handler == NULL) {
        return 0;
    }
    return handler->pending.count;
}

int urb_handler_completed_count(urb_handler_t *handler) {
    if (handler == NULL) {
        return 0;
    }
    return handler->completed.count;
}

bool urb_handler_is_running(urb_handler_t *handler) {
    return (handler != NULL && handler->running);
}

/* ----- Utility ----- */

uint32_t urb_handler_next_seqnum(urb_handler_t *handler) {
    if (handler == NULL) {
        return 0;
    }
    return InterlockedIncrement((volatile LONG*)&handler->next_seqnum);
}

urb_entry_t* urb_handler_find_pending(urb_handler_t *handler, uint32_t seqnum) {
    if (handler == NULL) {
        return NULL;
    }

    mutex_lock(&handler->pending.mutex);

    urb_entry_t *entry = handler->pending.head;
    while (entry != NULL) {
        if (entry->seqnum == seqnum) {
            mutex_unlock(&handler->pending.mutex);
            return entry;
        }
        entry = entry->next;
    }

    mutex_unlock(&handler->pending.mutex);
    return NULL;
}

void urb_handler_create_return(urb_entry_t *entry, usbip_header_t *ret_header) {
    if (entry == NULL || ret_header == NULL) {
        return;
    }

    memset(ret_header, 0, sizeof(usbip_header_t));

    ret_header->common.command = USBIP_RET_SUBMIT;
    ret_header->common.seqnum = entry->seqnum;
    ret_header->common.devid = entry->devid;
    ret_header->common.direction = entry->urb.direction;
    ret_header->common.ep = entry->urb.endpoint & 0x0F;  /* Remove direction bit */

    ret_header->u.ret_submit.status = entry->urb.status;
    ret_header->u.ret_submit.actual_length = entry->urb.actual_length;
    ret_header->u.ret_submit.start_frame = entry->urb.start_frame;
    ret_header->u.ret_submit.number_of_packets = entry->urb.number_of_packets;
    ret_header->u.ret_submit.error_count = 0;

    /* Echo setup packet */
    memcpy(ret_header->u.ret_submit.setup, entry->urb.setup, 8);
}

void urb_handler_print_urb(const urb_entry_t *entry) {
    if (entry == NULL) {
        return;
    }

    LOG_DEBUG("URB Entry:");
    LOG_DEBUG("  Seqnum: %u", entry->seqnum);
    LOG_DEBUG("  DevID: 0x%08X", entry->devid);
    LOG_DEBUG("  Endpoint: 0x%02X", entry->urb.endpoint);
    LOG_DEBUG("  Direction: %s", entry->urb.direction == USBIP_DIR_IN ? "IN" : "OUT");
    LOG_DEBUG("  Buffer Length: %u", entry->urb.buffer_length);
    LOG_DEBUG("  Actual Length: %u", entry->urb.actual_length);
    LOG_DEBUG("  Status: %d", entry->urb.status);
    LOG_DEBUG("  Completed: %s", entry->completed ? "Yes" : "No");
}
