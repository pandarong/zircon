// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <object/message_packet.h>

#include <err.h>
#include <stdint.h>
#include <string.h>

#include <zxcpp/new.h>
#include <object/handle.h>
#include <kernel/mutex.h>

constexpr size_t kBufferSize = 4096;

static mutex_t mutex = MUTEX_INITIAL_VALUE(mutex);

class MessagePacket::Buffer : public fbl::SinglyLinkedListable<Buffer*> {
public:
    Buffer() {}
    char b[0];

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(Buffer);
};

static MessagePacket::BufferList* buffer_list;

// static
void MessagePacket::AllocBuffers(BufferList* buffers, size_t num_buffers) {
    mutex_acquire(&mutex);
    for (size_t i = 0; i < num_buffers; ++i) {
        buffers->push_front(buffer_list->pop_front());
    }
    mutex_release(&mutex);
}

// static
zx_status_t MessagePacket::FillBuffers(BufferList* buffers, user_in_ptr<const void> data, size_t data_size) {
    size_t rem = data_size;
    for (auto iter = buffers->begin(); iter != buffers->end(); ++iter) {
        const size_t copy_len = fbl::min(rem, kBufferSize);
        const zx_status_t status = data.copy_array_from_user(iter->b, copy_len);
        assert(status == ZX_OK);
        data = data.byte_offset(copy_len);
        rem -= copy_len;
    }
    return ZX_OK;
}

// static
void MessagePacket::DeleteBufferList(BufferList* buffers) {
    mutex_acquire(&mutex);
    while (!buffers->is_empty()) {
        buffer_list->push_front(buffers->pop_front());
    }
    mutex_release(&mutex);
}

class Packet : public fbl::SinglyLinkedListable<Packet*> {
public:
    Packet() {}
    char mp[sizeof(MessagePacket) + kMaxMessageHandles * sizeof(Handle*)];

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(Packet);
};

typedef fbl::SinglyLinkedList<Packet*> MessagePacketList;
static MessagePacketList* packet_list;

static void* AllocPacket() {
    mutex_acquire(&mutex);
    Packet* node = packet_list->pop_front();
    mutex_release(&mutex);
    return &node->mp[0];
}

static void FreePacket(void* p) {
    mutex_acquire(&mutex);
    Packet* node = (Packet*)((char*)(p) - sizeof(fbl::SinglyLinkedListable<Packet*>));
    packet_list->push_front(node);
    mutex_release(&mutex);
}

// static
zx_status_t MessagePacket::NewMessagePacket(fbl::unique_ptr<MessagePacket>* msg, BufferList* buffers,
                                            uint32_t data_size, uint32_t num_handles) {
    fbl::AllocChecker ac;

    MessagePacket* mp = (MessagePacket*)AllocPacket();
    Handle** handles = (Handle**)((char*)mp + sizeof(MessagePacket));
    msg->reset(new (mp) MessagePacket(buffers, data_size, num_handles, handles));
    return ZX_OK;
}

// static
void MessagePacket::operator delete(void* ptr) {
    FreePacket(ptr);
}

// static
zx_status_t MessagePacket::Create(user_in_ptr<const void> data, uint32_t data_size,
                                  uint32_t num_handles,
                                  fbl::unique_ptr<MessagePacket>* msg) {

    static_assert(kMaxMessageHandles <= UINT16_MAX, "");
    if (data_size > kMaxMessageSize || num_handles > kMaxMessageHandles) {
        return ZX_ERR_OUT_OF_RANGE;
    }

    BufferList buffers;
    const size_t num_needed = (data_size / kBufferSize) + ((data_size % kBufferSize) > 0);

    MessagePacket::AllocBuffers(&buffers, num_needed);
    zx_status_t status = MessagePacket::FillBuffers(&buffers, data, data_size);
    if (status != ZX_OK) {
        return status;
    }
    fbl::unique_ptr<MessagePacket> new_msg;
    status = MessagePacket::NewMessagePacket(&new_msg, &buffers, data_size, num_handles);

    if (status != ZX_OK) {
        return status;
    }
    *msg = fbl::move(new_msg);
    return ZX_OK;
}

// static
zx_status_t MessagePacket::Create(const void* data, uint32_t data_size,
                                  uint32_t num_handles,
                                  fbl::unique_ptr<MessagePacket>* msg) {
    static_assert(kMaxMessageHandles <= UINT16_MAX, "");
    if (data_size > kMaxMessageSize || num_handles > kMaxMessageHandles) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    BufferList buffers;
    const size_t num_needed = (data_size / kBufferSize) + ((data_size % kBufferSize) > 0);

    MessagePacket::AllocBuffers(&buffers, num_needed);
    size_t rem = data_size;
    const char* src = static_cast<const char*>(data);
    for (auto iter = buffers.begin(); iter != buffers.end(); ++iter) {
        size_t copy_len = fbl::min(rem, kBufferSize);
        memcpy(iter->b, src, copy_len);
        src += copy_len;
        rem -= copy_len;
    }
    fbl::unique_ptr<MessagePacket> new_msg;
    zx_status_t status = MessagePacket::NewMessagePacket(&new_msg, &buffers, data_size, num_handles);

    if (status != ZX_OK) {
        return status;
    }
    *msg = fbl::move(new_msg);
    return ZX_OK;
}

zx_status_t MessagePacket::CopyDataTo(user_out_ptr<void> buf) const {
    size_t rem = data_size_;
    for (auto iter = buffers_.begin(); iter != buffers_.end(); ++iter) {
        const size_t len = fbl::min(rem, kBufferSize);
        const zx_status_t status = buf.copy_array_to_user(iter->b, len);
        if (status != ZX_OK) {
            return status;
        }
        buf = buf.byte_offset(len);
        rem -= len;
    }
    return ZX_OK;
}

MessagePacket::~MessagePacket() {
    if (owns_handles_) {
        for (size_t ix = 0; ix != num_handles_; ++ix) {
            // Delete the handle via HandleOwner dtor.
            HandleOwner ho(handles_[ix]);
        }
    }
    DeleteBufferList(&buffers_);
}

MessagePacket::MessagePacket(BufferList* buffers, uint32_t data_size,
                             uint32_t num_handles, Handle** handles)
    : buffers_(fbl::move(*buffers)), handles_(handles), data_size_(data_size),
      // NewPacket ensures that num_handles fits in 16 bits.
      num_handles_(static_cast<uint16_t>(num_handles)), owns_handles_(false) {
}

zx_txid_t MessagePacket::get_txid() const {
    if (data_size_ < sizeof(zx_txid_t)) {
        return 0;
    }

    // TODO(maniscalco): deal with strict aliasing issue
    const void* p = reinterpret_cast<const void*>(buffers_.front().b);
    zx_txid_t txid = *reinterpret_cast<const zx_txid_t*>(p);
    return txid;
}

void message_packet_init() {
    MessagePacket::Init();
}

// static
void MessagePacket::Init() {
    mutex_acquire(&mutex);

    fbl::AllocChecker ac;
    if (!buffer_list) {
        buffer_list = new (&ac) BufferList;
        assert(ac.check());
        for (int i = 0; i < 1000; ++i) {
            void* p = malloc(sizeof(Buffer) + kBufferSize);
            assert(p);
            new (p) Buffer;
            buffer_list->push_front((Buffer*)p);
        }
    }

    if (!packet_list) {
        packet_list = new (&ac) fbl::SinglyLinkedList<Packet*>;
        assert(ac.check());
        for (int i = 0; i < 1000; ++i) {
            Packet* p = new (&ac) Packet;
            assert(ac.check());
            assert(p);
            packet_list->push_front(p);
        }
    }

    mutex_release(&mutex);
}
