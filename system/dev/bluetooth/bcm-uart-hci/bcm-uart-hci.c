// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <ddk/binding.h>
#include <ddk/debug.h>
#include <ddk/device.h>
#include <ddk/driver.h>
#include <ddk/protocol/bt-hci.h>
#include <ddk/protocol/platform-defs.h>
#include <ddk/protocol/serial.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>
#include <zircon/device/bt-hci.h>
#include <zircon/status.h>
#include <zircon/threads.h>

// HCI reset
const uint8_t RESET_CMD[] = { 0x3, 0xc, 0x0 };

// vendor command to switch baud rate to 2000000
const uint8_t SET_BAUD_RATE_CMD[] = { 0x18, 0xfc, 0x6, 0x0, 0x0, 0x80, 0x84, 0x1e, 0x0 };

const uint8_t START_FIRMWARE_DOWNLOAD_CMD[] = { 0x2e, 0xfc, 0x0 };

#define HCI_EVT_COMMAND_COMPLETE    0x0e

typedef struct {
    zx_device_t* zxdev;
    zx_device_t* transport_dev;
    bt_hci_protocol_t hci;
    serial_protocol_t serial;
    zx_handle_t command_channel;
} bcm_uart_hci_t;

static zx_status_t bcm_hci_get_protocol(void* ctx, uint32_t proto_id, void* out_proto) {
    if (proto_id != ZX_PROTOCOL_BT_HCI) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    bcm_uart_hci_t* hci = ctx;
    bt_hci_protocol_t* hci_proto = out_proto;

    // Forward the underlying bt-transport ops.
    hci_proto->ops = hci->hci.ops;
    hci_proto->ctx = hci->hci.ctx;

    return ZX_OK;
}

static zx_status_t bcm_hci_ioctl(void* ctx, uint32_t op, const void* in_buf, size_t in_len,
                                 void* out_buf, size_t out_len, size_t* out_actual) {
    bcm_uart_hci_t* hci = ctx;
    if (out_len < sizeof(zx_handle_t)) {
        return ZX_ERR_BUFFER_TOO_SMALL;
    }

    zx_handle_t* reply = out_buf;

    zx_status_t status = ZX_ERR_NOT_SUPPORTED;
    if (op == IOCTL_BT_HCI_GET_COMMAND_CHANNEL) {
        if (hci->command_channel != ZX_HANDLE_INVALID) {
            *reply = hci->command_channel;
            hci->command_channel = ZX_HANDLE_INVALID;
            status = ZX_OK;
        } else {
            status = bt_hci_open_command_channel(&hci->hci, reply);
        }
    } else if (op == IOCTL_BT_HCI_GET_ACL_DATA_CHANNEL) {
        status = bt_hci_open_acl_data_channel(&hci->hci, reply);
    } else if (op == IOCTL_BT_HCI_GET_SNOOP_CHANNEL) {
        status = bt_hci_open_snoop_channel(&hci->hci, reply);
    }

    if (status != ZX_OK) {
        return status;
    }

    *out_actual = sizeof(*reply);
    return ZX_OK;
}

static void bcm_hci_unbind(void* ctx) {
    bcm_uart_hci_t* hci = ctx;

    device_remove(hci->zxdev);
}

static void bcm_hci_release(void* ctx) {
    free(ctx);
}

static zx_protocol_device_t bcm_hci_device_proto = {
    .version = DEVICE_OPS_VERSION,
    .get_protocol = bcm_hci_get_protocol,
    .ioctl = bcm_hci_ioctl,
    .unbind = bcm_hci_unbind,
    .release = bcm_hci_release,
};


static zx_status_t bcm_hci_send_command(bcm_uart_hci_t* hci, const uint8_t* command, size_t length) {
    // send HCI command
    zx_status_t status = zx_channel_write(hci->command_channel, 0, command, length, NULL, 0);
    if (status != ZX_OK) {
        zxlogf(ERROR, "bcm_hci_send_command zx_channel_write failed %d\n", status);
        return status;
    }

    // wait for command complete
    uint8_t event_buf[255 + 2];
    uint32_t actual;

    do {
        status = zx_channel_read(hci->command_channel, 0, event_buf, NULL, sizeof(event_buf), 0,
                                 &actual, NULL);
        if (status == ZX_ERR_SHOULD_WAIT) {
            zx_object_wait_one(hci->command_channel,  ZX_CHANNEL_READABLE | ZX_CHANNEL_PEER_CLOSED,
                               ZX_TIME_INFINITE, NULL);
        }
    } while (status == ZX_ERR_SHOULD_WAIT);

    if (status != ZX_OK) {
        zxlogf(ERROR, "bcm_hci_send_command zx_channel_read failed %d\n", status);
        return status;
    }

    if (event_buf[0] != HCI_EVT_COMMAND_COMPLETE || event_buf[1] != 4 || event_buf[3] != command[0]
        || event_buf[4] != command[1]) {
        zxlogf(ERROR, "bcm_hci_send_command did not receive command complete\n");
        return ZX_ERR_INTERNAL;
    }
    if (event_buf[5] != 0) {
        zxlogf(ERROR, "bcm_hci_send_command got command complete error %u\n", event_buf[5]);
        return ZX_ERR_INTERNAL;
    }

    return ZX_OK;
}

static int bcm_hci_start_thread(void* arg) {
    bcm_uart_hci_t* hci = arg;
    zx_handle_t fw_vmo;

    zx_status_t status = bt_hci_open_command_channel(&hci->hci, &hci->command_channel);
    if (status != ZX_OK) {
        goto fail;
    }

    // Send Reset command
    status = bcm_hci_send_command(hci, RESET_CMD, sizeof(RESET_CMD));
    if (status != ZX_OK) {
        goto fail;
    }

    // switch baud rate to 2000000
    status = bcm_hci_send_command(hci, SET_BAUD_RATE_CMD, sizeof(SET_BAUD_RATE_CMD));
    if (status != ZX_OK) {
        goto fail;
    }

    status = serial_config(&hci->serial, 0, 2000000, SERIAL_SET_BAUD_RATE_ONLY);
    if (status != ZX_OK) {
        goto fail;
    }

    size_t fw_size;
    status = load_firmware(hci->zxdev, "/boot/firmware/bt-firmware.bin", &fw_vmo, &fw_size);
    if (status == ZX_OK) {
        status = bcm_hci_send_command(hci, START_FIRMWARE_DOWNLOAD_CMD, sizeof(START_FIRMWARE_DOWNLOAD_CMD));
        if (status != ZX_OK) {
            goto fail;
        }

        zx_nanosleep(zx_deadline_after(ZX_MSEC(50)));

        zx_off_t offset = 0;
        while (offset < fw_size) {
            uint8_t buffer[255 + 3];
            size_t actual;

            status = zx_vmo_read(fw_vmo, buffer, offset, sizeof(buffer), &actual);
            if (status != ZX_OK) {
                goto vmo_close_fail;
            }
            if (actual < 3) {
                zxlogf(ERROR, "short HCI command in firmware download\n");
                status = ZX_ERR_INTERNAL;
                goto vmo_close_fail;
            }
            size_t length =  buffer[2] + 3;
             if (actual < length) {
                zxlogf(ERROR, "short HCI command in firmware download\n");
                status = ZX_ERR_INTERNAL;
                goto vmo_close_fail;
            }
            status = bcm_hci_send_command(hci, buffer, length);
            if (status != ZX_OK) {
                zxlogf(ERROR, "bcm_hci_send_command failed in firmware download: %d\n", status);
                goto vmo_close_fail;
            }
            offset += length;
        }

        zx_handle_close(fw_vmo);

        // firmware switched us back to 115200. switch back to 2000000
        status = serial_config(&hci->serial, 0, 115200, SERIAL_SET_BAUD_RATE_ONLY);
        if (status != ZX_OK) {
            goto fail;
        }

        // switch baud rate to 2000000 again
        status = bcm_hci_send_command(hci, SET_BAUD_RATE_CMD, sizeof(SET_BAUD_RATE_CMD));
        if (status != ZX_OK) {
            goto fail;
        }

        status = serial_config(&hci->serial, 0, 2000000, SERIAL_SET_BAUD_RATE_ONLY);
        if (status != ZX_OK) {
            goto fail;
        }
    } else {
        zxlogf(INFO, "bcm-uart-hci: no firmware file found\n");
    }

    device_make_visible(hci->zxdev);
    return 0;

vmo_close_fail:
    zx_handle_close(fw_vmo);
fail:
    zxlogf(ERROR, "bcm_hci_start_thread: device initialization failed: %d\n", status);
    device_remove(hci->zxdev);
    return -1;
}

static zx_status_t bcm_hci_bind(void* ctx, zx_device_t* device) {
    bcm_uart_hci_t* hci = calloc(1, sizeof(bcm_uart_hci_t));
    if (!hci) {
        return ZX_ERR_NO_MEMORY;
    }

    zx_status_t status = device_get_protocol(device, ZX_PROTOCOL_BT_HCI, &hci->hci);
    if (status != ZX_OK) {
        zxlogf(ERROR, "bcm_hci_bind: get protocol ZX_PROTOCOL_BT_HCI failed\n");
        return status;
    }
    status = device_get_protocol(device, ZX_PROTOCOL_SERIAL, &hci->serial);
    if (status != ZX_OK) {
        zxlogf(ERROR, "bcm_hci_bind: get protocol ZX_PROTOCOL_SERIAL failed\n");
        return status;
    }

    device_add_args_t args = {
        .version = DEVICE_ADD_ARGS_VERSION,
        .name = "bcm-uart-hci",
        .ctx = hci,
        .ops = &bcm_hci_device_proto,
        .proto_id = ZX_PROTOCOL_BT_HCI,
        .flags = DEVICE_ADD_INVISIBLE,
    };

    hci->transport_dev = device;

    status = device_add(device, &args, &hci->zxdev);
    if (status != ZX_OK) {
        bcm_hci_release(hci);
        return status;
    }

    // create thread to continue initialization
    thrd_t t;
    int thrd_rc = thrd_create_with_name(&t, bcm_hci_start_thread, hci, "bcm_hci_start_thread");
    if (thrd_rc != thrd_success) {
        device_remove(hci->zxdev);
        bcm_hci_release(hci);
        return thrd_status_to_zx_status(thrd_rc);
    }

    return ZX_OK;
}

static zx_driver_ops_t bcm_hci_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .bind = bcm_hci_bind,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(bcm_hci, bcm_hci_driver_ops, "fuchsia", "0.1", 3)
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_BT_TRANSPORT),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_BROADCOM),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_PID, PDEV_PID_BCM4356),
ZIRCON_DRIVER_END(bcm_hci)
