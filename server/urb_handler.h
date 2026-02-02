/*
 * USB Over Network - URB Handler
 * Windows-only implementation
 *
 * USB Request Block processing and queue management
 */

#ifndef URB_HANDLER_H
#define URB_HANDLER_H

#include "../common/types.h"
#include "../common/error.h"
#include "../common/protocol.h"
#include "usb_host.h"

/* Maximum pending URBs per device */
#define MAX_PENDING_URBS_PER_DEVICE 64

/* URB queue entry */
typedef struct urb_entry {
    usb_urb_t urb;                  /* URB data */
    uint32_t seqnum;                /* Sequence number */
    uint32_t devid;                 /* Device ID */
    uint64_t submit_time;           /* When URB was submitted */
    bool in_progress;               /* Currently being processed */
    bool completed;                 /* Processing complete */
    struct urb_entry *next;         /* Next entry */
} urb_entry_t;

/* URB queue for a device */
typedef struct urb_queue {
    urb_entry_t *head;              /* Queue head */
    urb_entry_t *tail;              /* Queue tail */
    int count;                      /* Number of entries */
    mutex_t mutex;                  /* Thread safety */
} urb_queue_t;

/* URB handler context */
typedef struct urb_handler {
    usb_device_t *device;           /* Associated USB device */
    urb_queue_t pending;            /* Pending URB queue */
    urb_queue_t completed;          /* Completed URB queue */
    uint32_t next_seqnum;           /* Next sequence number */
    bool running;                   /* Handler is running */
    thread_t worker_thread;         /* Worker thread */
    bool initialized;               /* Initialization flag */
} urb_handler_t;

/* ----- Initialization ----- */

/* Initialize URB handler */
error_code_t urb_handler_init(urb_handler_t *handler, usb_device_t *device);

/* Cleanup URB handler */
void urb_handler_cleanup(urb_handler_t *handler);

/* ----- URB Processing ----- */

/* Process incoming URB submit command */
error_code_t urb_handler_submit(urb_handler_t *handler,
                                 const usbip_header_t *header,
                                 const uint8_t *data, uint32_t data_len);

/* Process URB unlink command */
error_code_t urb_handler_unlink(urb_handler_t *handler, uint32_t seqnum_to_unlink);

/* Get next completed URB */
urb_entry_t* urb_handler_get_completed(urb_handler_t *handler);

/* Free completed URB entry */
void urb_handler_free_entry(urb_entry_t *entry);

/* ----- Control ----- */

/* Start URB processing */
error_code_t urb_handler_start(urb_handler_t *handler);

/* Stop URB processing */
void urb_handler_stop(urb_handler_t *handler);

/* Cancel all pending URBs */
void urb_handler_cancel_all(urb_handler_t *handler);

/* ----- Status ----- */

/* Get pending URB count */
int urb_handler_pending_count(urb_handler_t *handler);

/* Get completed URB count */
int urb_handler_completed_count(urb_handler_t *handler);

/* Check if handler is running */
bool urb_handler_is_running(urb_handler_t *handler);

/* ----- Utility ----- */

/* Generate next sequence number */
uint32_t urb_handler_next_seqnum(urb_handler_t *handler);

/* Find pending URB by sequence number */
urb_entry_t* urb_handler_find_pending(urb_handler_t *handler, uint32_t seqnum);

/* Create URB return header */
void urb_handler_create_return(urb_entry_t *entry, usbip_header_t *ret_header);

/* Debug print URB info */
void urb_handler_print_urb(const urb_entry_t *entry);

#endif /* URB_HANDLER_H */
