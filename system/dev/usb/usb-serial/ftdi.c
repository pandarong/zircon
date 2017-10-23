// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/binding.h>
#include <driver/usb.h>
#include <sync/completion.h>
#include <ddk/device.h>
#include <ddk/driver.h>

#include <ddk/protocol/usb.h>
#include <zircon/listnode.h>
#include <zircon/device/usb.h>

#include <inttypes.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "ftdi.h"

#define FTDI_STATUS_SIZE 2
#define FTDI_RX_HEADER_SIZE 4

#define READ_REQ_COUNT 8
#define WRITE_REQ_COUNT 4
#define INTR_REQ_COUNT 4
#define USB_BUF_SIZE 2048
#define INTR_REQ_SIZE 4

#define FIFOSIZE 256
#define FIFOMASK (FIFOSIZE - 1)

typedef struct {
    zx_device_t* device;
    zx_device_t* usb_device;
    zx_driver_t* driver;
    uint16_t ftditype;                  //FTDI device type
    uint32_t baudrate;

    uint8_t status[INTR_REQ_SIZE];

    // pool of free USB requests
    list_node_t free_read_reqs;
    list_node_t free_write_reqs;

    // list of received packets not yet read by upper layer
    list_node_t completed_reads;
    // the last signals we reported
    //zx_signals_t signals;

    size_t read_offset;

    mtx_t mutex;
} ftdi_t;


/*
#define get_ftdi(dev) ((ftdi_t*)dev->ctx)

static void update_signals_locked(ftdi_t* ftdi) {
    mx_signals_t new_signals = 0;

 //   if (eth->dead)
 //       new_signals |= (DEV_STATE_READABLE | DEV_STATE_ERROR);
    if (!list_is_empty(&ftdi->completed_reads))
        new_signals |= DEV_STATE_READABLE;
    if (!list_is_empty(&ftdi->free_write_reqs))
        new_signals |= DEV_STATE_WRITABLE;
    if (new_signals != ftdi->signals) {
        //device_state_set_clr(ftdi->device, new_signals & ~ftdi->signals, ftdi->signals & ~new_signals);
        ftdi->signals = new_signals;
    }
}

static void requeue_read_request_locked(ftdi_t* ftdi, iotxn_t* req) {
        iotxn_queue(ftdi->usb_device, req);
}



static void ftdi_read_complete(iotxn_t* request, void* cookie) {
    ftdi_t* ftdi = (ftdi_t*)cookie;
    //printf("FTDI: read complete\n");

    if (request->status == ERR_REMOTE_CLOSED) {
        printf("FTDI: remote closed\n");
        request->ops->release(request);
        return;
    }

    mtx_lock(&ftdi->mutex);
    if ((request->status == NO_ERROR) && (request->actual > 2)) {
        //printf("FTDI: read complete\n");
        list_add_tail(&ftdi->completed_reads, &request->node);
    } else {
        requeue_read_request_locked(ftdi, request);
    }
    //update_signals_locked(ftdi);
    mtx_unlock(&ftdi->mutex);
}

static void ftdi_write_complete(iotxn_t* request, void* cookie) {
    //printf("ftdi write complete -  status=%d\n",request->status);
    ftdi_t* ftdi = (ftdi_t*)cookie;
    if (request->status == ERR_REMOTE_CLOSED) {
        request->ops->release(request);
        return;
    }

    mtx_lock(&ftdi->mutex);
    list_add_tail(&ftdi->free_write_reqs, &request->node);
    update_signals_locked(ftdi);
    mtx_unlock(&ftdi->mutex);
}

static mx_status_t ftdi_calc_dividers(uint32_t* baudrate, uint32_t clock,       uint32_t divisor,
                                                          uint16_t* integer_div, uint16_t* fraction_div) {

    static const uint8_t frac_lookup[8] = {0, 3, 2, 4, 1, 5, 6, 7};

    uint32_t base_clock = clock/divisor;

    // integer dividers of 1 and 0 are special cases.  0=base_clock and 1 = 2/3 of base clock
    if (*baudrate >=  base_clock) {  // return with max baud rate achievable
        *fraction_div = 0;
        *integer_div = 0;
        *baudrate = base_clock;
    }
    else if (*baudrate >=  (base_clock* 2 )/3) {
        *integer_div = 1;
        *fraction_div = 0;
        *baudrate = (base_clock * 2)/3;
    } else {
        // create a 28.4 fractional integer
        uint32_t ratio = (base_clock * 16) / *baudrate;
        ratio++;    //round up if needed
        ratio = ratio & 0xfffffffe;

        *baudrate = (base_clock << 4) / ratio;
        *integer_div = ratio >> 4;
        *fraction_div = frac_lookup[ (ratio >> 1) & 0x07 ];
    }
    return NO_ERROR;
}

static ssize_t ftdi_write(mx_device_t* dev, const void* data, size_t len, mx_off_t off) {
//mx_status_t ftdi_write(ftdi_t* ftdi, const uint8_t* buff, size_t length) {
    ftdi_t* ftdi =get_ftdi(dev);
    uint8_t* buff = (uint8_t*)data;

    mx_status_t status=NO_ERROR;
    mtx_lock(&ftdi->mutex);

    list_node_t* node = list_remove_head(&ftdi->free_write_reqs);
    if (!node) {
        printf("shit broke yo...\n");
        status = ERR_BUFFER_TOO_SMALL;
        goto out;
    }
    iotxn_t* request = containerof(node, iotxn_t, node);
    if (!request) {
        printf("null request node\n");
        goto out;
    }
    request->ops->copyto(request, buff, len, 0);
    request->length=len;
    iotxn_queue(ftdi->usb_device, request);
out:
    update_signals_locked(ftdi);
    mtx_unlock(&ftdi->mutex);
    return status;
}

static ssize_t ftdi_read(mx_device_t* dev, void* data, size_t len, mx_off_t off) {
    ftdi_t* ftdi = get_ftdi(dev);

    size_t bytes_copied = 0;
    size_t offset = ftdi->read_offset;
    uint8_t* buffer = (uint8_t*)data;

    mtx_lock(&ftdi->mutex);

    list_node_t* node = list_peek_head(&ftdi->completed_reads);
    while ((node) && (bytes_copied < len)) {

        iotxn_t* request = containerof(node, iotxn_t, node);
        size_t to_copy = request->actual - offset - FTDI_STATUS_SIZE;
        //printf("actual=%zu  offset=%zu\n",request->actual, offset);
        if ( (to_copy + bytes_copied) > len) {
            to_copy = len - bytes_copied;
        }

        request->ops->copyfrom(request, &buffer[bytes_copied], to_copy, offset + FTDI_STATUS_SIZE);
        bytes_copied = bytes_copied + to_copy;
        if ((to_copy + offset + FTDI_STATUS_SIZE) < request->actual) {
            offset = offset + to_copy;
            goto out;
        } else {
            list_remove_head(&ftdi->completed_reads);
            requeue_read_request_locked(ftdi, request);
            offset = 0;
        }

        node = list_peek_head(&ftdi->completed_reads);
    }
out:
    ftdi->read_offset = offset;
    update_signals_locked(ftdi);
    mtx_unlock(&ftdi->mutex);
    return bytes_copied;
}

static mx_status_t ftdi_set_baudrate(ftdi_t* ftdi, uint32_t baudrate){
    uint16_t whole,fraction,value,index;
    mx_status_t status;

    if (ftdi == NULL) return ERR_INVALID_ARGS;
    switch(ftdi->ftditype) {
        case FTDI_TYPE_R:
        case FTDI_TYPE_2232C:
        case FTDI_TYPE_BM:
            ftdi_calc_dividers(&baudrate,FTDI_C_CLK,16,&whole,&fraction);
            ftdi->baudrate = baudrate;
            break;
        default:
            return ERR_INVALID_ARGS;
    }
    value = (whole & 0x3fff) | (fraction << 14);
    index = fraction >> 2;
    status = usb_control(ftdi->usb_device, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                         FTDI_SIO_SET_BAUDRATE, value, index, NULL, 0);

    //supported_baud = getbaud

    return status;
}

static mx_status_t ftdi_reset(ftdi_t* ftdi) {

    if (ftdi == NULL || ftdi->usb_device == NULL)
        return ERR_INVALID_ARGS;

    return usb_control(ftdi->usb_device, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE,
                         FTDI_SIO_RESET_REQUEST, FTDI_SIO_RESET, 0, NULL, 0);
}

const char teststr[]="Eric0123456789abcdefghijklmnop";

static mx_protocol_device_t ftdi_device_proto = {
    //.unbind = ax88772b_unbind,
    //.release = ax88772b_release,
    .read = ftdi_read,
    .write = ftdi_write,
};

static int ftdi_start_thread(void* arg) {
    ftdi_t* ftdi = (ftdi_t*)arg;
    mx_status_t status = 0;
    printf("Initializing FTDI...\n");
    if (ftdi_reset(ftdi) < 0) {
        printf("reset failed\n");
    } else {
        printf("FTDI: reset complete\n");
    }

    status = ftdi_set_baudrate(ftdi, 115200);
    if (status < 0) {
        printf("FTDI: set baudrate failed\n");
    }

    printf("FTDI: set baudrate complete\n");



    status = device_create(&ftdi->device, ftdi->driver, "usb-serial", &ftdi_device_proto);
    if (status < 0) {
        printf("FTDI: failed to create device: %d\n", status);
        return 0;
    }

    status = device_add(ftdi->device, ftdi->usb_device);
    if (status != NO_ERROR) {
        free(ftdi->device);
        return status;
    }
    status = device_add(ftdi->device, driver_get_misc_device());

    iotxn_t* req;
    iotxn_t* prev;
    list_for_every_entry_safe (&ftdi->free_read_reqs, req, prev, iotxn_t, node) {
        list_delete(&req->node);
        requeue_read_request_locked(ftdi, req);
    }
    update_signals_locked(ftdi);

#if 0
    size_t numbytes;
    uint8_t inbuff[100];
    while(1) {
        status = ftdi_write(ftdi,(uint8_t*)teststr,30);
        printf("did write\n");
        if (status < 0) printf("FTDI: write failed\n");
        mx_nanosleep(MX_MSEC(1000));
        numbytes =  ftdi_read(ftdi,inbuff,10);
        printf("Read #1 - read %zu bytes: ",numbytes);
        for (uint i=0; i<numbytes; i++)
            printf("%c",inbuff[i]);
        printf("\n");
        numbytes =  ftdi_read(ftdi,inbuff,100);
        printf("Read #1 - read %zu bytes: ",numbytes);
        for (uint i=0; i<numbytes; i++)
            printf("%c",inbuff[i]);
        printf("\n");
        mx_nanosleep(MX_MSEC(1000));

    }
#endif
    return 0;
}
*/


static zx_status_t ftdi_bind(void* ctx, zx_device_t* device, void** cookie) {

    printf("FTDI: usbserial - attempting to bind\n");


    usb_protocol_t usb;
    zx_status_t result = device_get_protocol(device, ZX_PROTOCOL_USB, &usb);
    if (result != ZX_OK) {
        return result;
    }

    // find our endpoints
    usb_desc_iter_t iter;
    zx_status_t status = ZX_OK;
    result = usb_desc_iter_init(&usb, &iter);
    if (result < 0)
        return result;

    usb_interface_descriptor_t* intf = usb_desc_iter_next_interface(&iter, true);
    printf("FTDI: returned %d endpoints\n", intf->bNumEndpoints);

    //uint8_t bulk_in_addr = 0;
    //uint8_t bulk_out_addr = 0;
/*
    usb_endpoint_descriptor_t* endp = usb_desc_iter_next_endpoint(&iter);
    while (endp) {
        if (usb_ep_direction(endp) == USB_ENDPOINT_OUT) {
            if (usb_ep_type(endp) == USB_ENDPOINT_BULK) {
                bulk_out_addr = endp->bEndpointAddress;
                printf("lan9514 bulk out endpoint:%x\n", bulk_out_addr);
            }
        } else {
            if (usb_ep_type(endp) == USB_ENDPOINT_BULK) {
                bulk_in_addr = endp->bEndpointAddress;
                printf("lan9514 bulk in endpoint:%x\n", bulk_in_addr);
            }
        }
        endp = usb_desc_iter_next_endpoint(&iter);
    }

    usb_desc_iter_release(&iter);

    if (!bulk_in_addr || !bulk_out_addr ) {
        printf("FTDI: could not find all endpoints\n");
        return ERR_NOT_SUPPORTED;
    }

    ftdi_t* ftdi = calloc(1, sizeof(ftdi_t));
    if (!ftdi) {
        printf("FTDI: Not enough memory\n");
        printf("FTDI: bind failed!\n");
        return ERR_NO_MEMORY;
    }
    usb_device_descriptor_t devdesc;
    result = device->ops->ioctl(device, IOCTL_USB_GET_DEVICE_DESC, NULL, 0, &devdesc, sizeof(devdesc));

    ftdi->ftditype = devdesc.bcdDevice;
    printf("FTDI: Device type = %04x\n",ftdi->ftditype);

    list_initialize(&ftdi->free_read_reqs);
    list_initialize(&ftdi->free_write_reqs);
    list_initialize(&ftdi->completed_reads);

    ftdi->usb_device = device;
    ftdi->driver = driver;


    for (int i = 0; i < READ_REQ_COUNT; i++) {
        iotxn_t* req = usb_alloc_iotxn(bulk_in_addr, USB_BUF_SIZE, 0);
        if (!req) {
            status = ERR_NO_MEMORY;
            goto fail;
        }
        req->length = USB_BUF_SIZE;
        req->complete_cb = ftdi_read_complete;
        req->cookie = ftdi;
        list_add_head(&ftdi->free_read_reqs, &req->node);
    }
    for (int i = 0; i < WRITE_REQ_COUNT; i++) {
        iotxn_t* req = usb_alloc_iotxn(bulk_out_addr, USB_BUF_SIZE, 0);
        if (!req) {
            status = ERR_NO_MEMORY;
            goto fail;
        }
        req->length = USB_BUF_SIZE;
        req->complete_cb = ftdi_write_complete;
        req->cookie = ftdi;
        list_add_head(&ftdi->free_write_reqs, &req->node);
    }

    printf("ftdi bind successful?\n");

#if 1
    thrd_t thread;
    thrd_create_with_name(&thread, ftdi_start_thread, ftdi, "ftdi_start_thread");
    thrd_detach(thread);
    return NO_ERROR;
#endif
fail:
    printf("ftdi_bind failed: %d\n", status);
    //ftdi_free(ftdi);
    return status;
*/
    return status;
}



static zx_driver_ops_t ftdi_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = ftdi_bind,
};

ZIRCON_DRIVER_BEGIN(driver_ftdi,ftdi_driver_ops, "zircon", "0.1", 2)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_USB),
    BI_MATCH_IF(EQ, BIND_USB_VID, FTDI_VID),
//    BI_MATCH_IF(EQ, BIND_USB_PID, FTDI_232_PID),
ZIRCON_DRIVER_END(driver_ftdi)