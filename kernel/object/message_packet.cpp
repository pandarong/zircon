// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <object/message_packet.h>

#include <err.h>
#include <stdint.h>
#include <string.h>

#include <fbl/atomic.h>
#include <object/handle_reaper.h>
#include <zxcpp/new.h>

#define ZIRCON_CHANNEL_STATS 1
#if ZIRCON_CHANNEL_STATS
#include <inttypes.h>
#include <lib/heap.h>
#include <platform.h>
namespace {
static fbl::atomic<lk_time_t> next_dump_time(0);

static fbl::atomic_uint_fast64_t total_data_size(0);
static fbl::atomic_uint_fast64_t high_water(0);
void _incr_data_size(uint64_t data_size) {
    uint64_t total = total_data_size.fetch_add(data_size) + data_size;
    uint64_t hwm = high_water.load();
    if (total > hwm) {
        high_water.compare_exchange_strong(&hwm, total,
                                           fbl::memory_order_seq_cst,
                                           fbl::memory_order_seq_cst);
        hwm = total;
    }

    lk_time_t now = current_time();
    if (hwm == total || now >= next_dump_time.load()) {
        next_dump_time.store(now + ZX_SEC(1));
        size_t heap_size;
        size_t heap_free;
        heap_get_info(&heap_size, &heap_free);
        printf("CHAN: t=%" PRIu64 " ds=%" PRIu64 " hwm=%" PRIu64 " hs=%zu\n",
               now, total, hwm, heap_size - heap_free);
    }
}
#define incr_data_size(data_size) (_incr_data_size(data_size))
#define decr_data_size(data_size) (total_data_size.fetch_sub(data_size))
} // namespace
#else // !ZIRCON_CHANNEL_STATS
#define incr_data_size(data_size) ((void)data_size)
#define decr_data_size(data_size) ((void)data_size)
#endif

// static
zx_status_t MessagePacket::NewPacket(uint32_t data_size, uint32_t num_handles,
                                     fbl::unique_ptr<MessagePacket>* msg) {
    // Although the API uses uint32_t, we pack the handle count into a smaller
    // field internally. Make sure it fits.
    static_assert(kMaxMessageHandles <= UINT16_MAX, "");
    if (data_size > kMaxMessageSize || num_handles > kMaxMessageHandles) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    // Allocate space for the MessagePacket object followed by num_handles
    // Handle*s followed by data_size bytes.
    // TODO(dbort): Use mbuf-style memory for data_size, ideally allocating from
    // somewhere other than the heap. Lets us better track and isolate channel
    // memory usage.
    char* ptr = static_cast<char*>(malloc(sizeof(MessagePacket) +
                                          num_handles * sizeof(Handle*) +
                                          data_size));
    if (ptr == nullptr) {
        return ZX_ERR_NO_MEMORY;
    }
    incr_data_size(data_size);

    // The storage space for the Handle*s is not initialized because
    // the only creators of MessagePackets (sys_channel_write and
    // _call, and userboot) fill that array immediately after creation
    // of the object.
    msg->reset(new (ptr) MessagePacket(
        data_size, num_handles,
        reinterpret_cast<Handle**>(ptr + sizeof(MessagePacket))));
    return ZX_OK;
}

// static
zx_status_t MessagePacket::Create(user_ptr<const void> data, uint32_t data_size,
                                  uint32_t num_handles,
                                  fbl::unique_ptr<MessagePacket>* msg) {
    zx_status_t status = NewPacket(data_size, num_handles, msg);
    if (status != ZX_OK) {
        return status;
    }
    if (data_size > 0u) {
        if (data.copy_array_from_user((*msg)->data(), data_size) != ZX_OK) {
            msg->reset();
            return ZX_ERR_INVALID_ARGS;
        }
    }
    return ZX_OK;
}

// static
zx_status_t MessagePacket::Create(const void* data, uint32_t data_size,
                                  uint32_t num_handles,
                                  fbl::unique_ptr<MessagePacket>* msg) {
    zx_status_t status = NewPacket(data_size, num_handles, msg);
    if (status != ZX_OK) {
        return status;
    }
    if (data_size > 0u) {
        memcpy((*msg)->data(), data, data_size);
    }
    return ZX_OK;
}

MessagePacket::~MessagePacket() {
    if (owns_handles_) {
        // Delete handles out-of-band to avoid the worst case recursive
        // destruction behavior.
        ReapHandles(handles_, num_handles_);
    }
    decr_data_size(data_size_);
}

MessagePacket::MessagePacket(uint32_t data_size,
                             uint32_t num_handles, Handle** handles)
    : handles_(handles), data_size_(data_size),
      // NewPacket ensures that num_handles fits in 16 bits.
      num_handles_(static_cast<uint16_t>(num_handles)), owns_handles_(false) {
}
