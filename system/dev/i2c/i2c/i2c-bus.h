// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <threads.h>

#include <ddk/protocol/i2c.h>
#include <ddktl/protocol/i2c-impl.h>
#include <fbl/mutex.h>
#include <zircon/listnode.h>

namespace i2c {

class I2cBus {
public:
    explicit I2cBus(ddk::I2cImplProtocolProxy i2c_impl, uint32_t bus_id);
    zx_status_t Start();

    zx_status_t Transact(const void* write_buf, size_t write_length, size_t read_length,
                         i2c_complete_cb complete_cb, void* cookie);

    inline size_t GetMaxTransferSize() const { return max_transfer_size_; }

private:
    // struct representing an I2C transaction.
    struct I2cTxn {
        uint32_t txid;
        zx_handle_t channel_handle;

        list_node_t node;
        size_t write_length;
        size_t read_length;
        uint16_t address;
        i2c_complete_cb complete_cb;
        void* cookie;
        uint8_t write_buffer[];
    };

    void Complete(I2cTxn* txn, zx_status_t status, const uint8_t* data,
                  size_t data_length);
    int I2cThread();

    ddk::I2cImplProtocolProxy i2c_impl_;
    const uint32_t bus_id_;
    size_t max_transfer_size_;

    list_node_t queued_txns_ __TA_GUARDED(mutex_);
    list_node_t free_txns_ __TA_GUARDED(mutex_);
    sync_completion_t txn_signal_;

    thrd_t thread_;
    fbl::Mutex mutex_;
};

} // namespace i2c
