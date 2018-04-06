// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <stdint.h>

#include <lib/user_copy/user_ptr.h>
#include <zircon/types.h>
#include <fbl/intrusive_double_list.h>
#include <fbl/intrusive_single_list.h>
#include <fbl/unique_ptr.h>

constexpr uint32_t kMaxMessageSize = 65536u;
constexpr uint32_t kMaxMessageHandles = 64u;

// ensure public constants are aligned
static_assert(ZX_CHANNEL_MAX_MSG_BYTES == kMaxMessageSize, "");
static_assert(ZX_CHANNEL_MAX_MSG_HANDLES == kMaxMessageHandles, "");

class Handle;

class MessagePacket : public fbl::DoublyLinkedListable<fbl::unique_ptr<MessagePacket>> {
public:
    // Creates a message packet containing the provided data and space for
    // |num_handles| handles. The handles array is uninitialized and must
    // be completely overwritten by clients.
    static zx_status_t Create(user_in_ptr<const void> data, uint32_t data_size,
                              uint32_t num_handles,
                              fbl::unique_ptr<MessagePacket>* msg);
    static zx_status_t Create(const void* data, uint32_t data_size,
                              uint32_t num_handles,
                              fbl::unique_ptr<MessagePacket>* msg);

    uint32_t data_size() const { return data_size_; }

    // Copies the packet's |data_size()| bytes to |buf|.
    // Returns an error if |buf| points to a bad user address.
    zx_status_t CopyDataTo(user_out_ptr<void> buf) const;

    uint32_t num_handles() const { return num_handles_; }
    Handle* const* handles() const { return handles_; }
    Handle** mutable_handles() { return handles_; }

    void set_owns_handles(bool own_handles) { owns_handles_ = own_handles; }

    // zx_channel_call treats the leading bytes of the payload as
    // a transaction id of type zx_txid_t.
    zx_txid_t get_txid() const;

    struct Buffer;

private:
    typedef fbl::SinglyLinkedList<Buffer*> BufferList;

    MessagePacket(BufferList* buffers, uint32_t data_size, uint32_t num_handles, Handle** handles);
    ~MessagePacket();

    template <typename PTR>
    static zx_status_t CreateCommon(PTR data, uint32_t data_size, uint32_t num_handles,
                                    fbl::unique_ptr<MessagePacket>* msg);

    static void Free(void* p);

    friend class fbl::unique_ptr<MessagePacket>;
    static void operator delete(void* ptr);

    BufferList* buffers_;
    Handle** const handles_;
    const uint32_t data_size_;
    const uint16_t num_handles_;
    bool owns_handles_;
};
