// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <object/message_packet.h>

#include <err.h>
#include <kernel/mutex.h>
#include <object/handle.h>
#include <stdint.h>
#include <string.h>
#include <trace.h>
#include <zxcpp/new.h>

constexpr size_t kOverallSize = 4096;
// TODO(maniscalco): Figure out the real malloc overhead.
constexpr size_t kMallocHeader = 32;
constexpr size_t kOverhead = kMallocHeader +
                             sizeof(fbl::SinglyLinkedListable<MessagePacket::Buffer*>);
constexpr size_t kPayloadSize = kOverallSize - kOverhead;

struct MessagePacket::Buffer final : public fbl::SinglyLinkedListable<Buffer*> {
    char data[kPayloadSize];
};
static_assert(sizeof(MessagePacket::Buffer) + kMallocHeader == kOverallSize, "");

// FreeList is a free list of |T|* containing at most |MaxSize|.
//
// It's currently backed by the heap (malloc/free).
template <typename T, size_t MaxSize>
class FreeList {
public:
    typedef fbl::SinglyLinkedList<T*> ListType;

    // Allocates |count| objects and return them on |result|.
    //
    // If a non-empty |result| is passed in, its elements may be removed and freed.
    //
    // Returns ZX_OK if successful. On error, result is unmodified.
    zx_status_t Alloc(size_t count, ListType* result) {
        ListType list;
        for (size_t i = 0; i < count; ++i) {
            if (unlikely(free_list_.is_empty())) {
                for (size_t i = 0; i < MaxSize; ++i) {
                    T* t = static_cast<T*>(malloc(sizeof(T)));
                    if (unlikely(t == nullptr)) {
                        Free(&list);
                        return ZX_ERR_NO_MEMORY;
                    }
                    new (t) T;
                    free_list_.push_front(t);
                    ++num_elements_;
                }
            }
            list.push_front(free_list_.pop_front());
            --num_elements_;
        }
        list.swap(*result);
        return ZX_OK;
    }

    // Frees |t|.
    void FreeOne(T* t) {
        t->~T();
        if (num_elements_ < MaxSize) {
            new (t) T;
            free_list_.push_front(t);
            ++num_elements_;
        } else {
            free(t);
        }
    }

    // Frees all elements of |list|.
    void Free(ListType* list) {
        while (!list->is_empty()) {
            T* t = list->pop_front();
            FreeOne(t);
        }
    }

private:
    size_t num_elements_ = 0;
    ListType free_list_;
};

class Node;
typedef FreeList<Node, 64> NodeFreeList;
typedef FreeList<MessagePacket::Buffer, 64> BufferFreeList;

fbl::Mutex mutex;
static NodeFreeList node_free_list TA_GUARDED(mutex);
static BufferFreeList buffer_free_list TA_GUARDED(mutex);

// Node holds a MessagePacket and its MessagePacket::Buffers.
//
// Nodes and MessagePacket::Buffers are allocated from free lists.
class Node : public fbl::SinglyLinkedListable<Node*> {
public:
    // Creates a Node with enough buffers to store |data_size| bytes.
    //
    // It is the callers responsibility to free the node with Node::Free.
    //
    // Does not construct the MessagePacket contained with in the node.
    //
    // On error, returns nullptr.
    static Node* Create(uint32_t data_size) {
        const size_t num_buffers = (data_size / kPayloadSize) + ((data_size % kPayloadSize) > 0);
        NodeFreeList::ListType n;

        fbl::AutoLock guard(&mutex);

        if (unlikely(node_free_list.Alloc(1, &n) != ZX_OK)) {
            return nullptr;
        }
        if (unlikely(buffer_free_list.Alloc(num_buffers, n.front().buffers()) != ZX_OK)) {
            // Free any buffers that were successfully allocated and then the node.
            buffer_free_list.Free(n.front().buffers());
            node_free_list.Free(&n);
            return nullptr;
        }
        return n.pop_front();
    }

    // Frees |node|.
    static void Free(Node* node) {
        fbl::AutoLock guard(&mutex);
        buffer_free_list.Free(node->buffers());
        node_free_list.FreeOne(node);
    }

    BufferFreeList::ListType* buffers() { return &buffers_; }
    MessagePacket* packet() { return reinterpret_cast<MessagePacket*>(packet_); }
    Handle** handles() { return handles_; }

private:
    friend zx_status_t NodeFreeList::Alloc(size_t count, ListType* result);
    friend void NodeFreeList::FreeOne(Node *t);
    Node() {}

    friend class fbl::unique_ptr<Node>;
    static void operator delete(void* ptr) {
        Node::Free(static_cast<Node*>(ptr));
    }

    char packet_[sizeof(MessagePacket)];
    BufferFreeList::ListType buffers_;
    Handle* handles_[kMaxMessageHandles];

    DISALLOW_COPY_ASSIGN_AND_MOVE(Node);
};
static_assert(sizeof(Node) + kMallocHeader <= kOverallSize, "");

void MessagePacket::Free(void* p) {
    // p points to a MessagePacket. Find its enclosing Node and Node::Free it.
    Node* node = reinterpret_cast<Node*>(
        static_cast<char*>(p) - sizeof(fbl::SinglyLinkedListable<Node*>));
    DEBUG_ASSERT(node->packet() == p);
    Node::Free(node);
}

// static
void MessagePacket::operator delete(void* ptr) {
    MessagePacket::Free(ptr);
}

// |PTR_IN| is a user_in_ptr-like type.
template <typename PTR_IN>
// static
zx_status_t MessagePacket::CreateCommon(PTR_IN data, uint32_t data_size, uint32_t num_handles,
                                        fbl::unique_ptr<MessagePacket>* msg) {
    static_assert(kMaxMessageHandles <= UINT16_MAX, "");
    if (unlikely(data_size > kMaxMessageSize || num_handles > kMaxMessageHandles)) {
        return ZX_ERR_OUT_OF_RANGE;
    }
    fbl::unique_ptr<Node> node(Node::Create(data_size));
    if (unlikely(!node)) {
        return ZX_ERR_NO_MEMORY;
    }

    size_t rem = data_size;
    for (auto iter = node->buffers()->begin(); iter != node->buffers()->end(); ++iter) {
        const size_t copy_len = fbl::min(rem, kPayloadSize);
        const zx_status_t status = data.copy_array_from_user(iter->data, copy_len);
        if (unlikely(status != ZX_OK)) {
            return status;
        }
        data = data.byte_offset(copy_len);
        rem -= copy_len;
    }

    // Construct the MessagePacket into the Node.
    new (node->packet()) MessagePacket(node->buffers(), data_size, num_handles, node->handles());
    msg->reset(node->packet());
    // Now that msg owns the MessagePacket, release the enclosing Node from node.
    __UNUSED auto ptr = node.release();

    return ZX_OK;
}

// static
zx_status_t MessagePacket::Create(user_in_ptr<const void> data, uint32_t data_size,
                                  uint32_t num_handles,
                                  fbl::unique_ptr<MessagePacket>* msg) {
    return MessagePacket::CreateCommon(data, data_size, num_handles, msg);
}

// Makes a const void* look like a user_in_ptr<const void>.
//
// MessagePacket has two overloads of Create.  One that operates on a user_in_ptr and one that
// operates on a kernel-space void*. KernelPtrAdapter allows us to implement the Create logic once
// (CreateCommon) for both these overloads.
class KernelPtrAdapter {
public:
    explicit KernelPtrAdapter(const void* p)
        : p_(p) {}

    zx_status_t copy_array_from_user(void* dst, size_t count) const {
        memcpy(dst, p_, count);
        return ZX_OK;
    }

    KernelPtrAdapter byte_offset(size_t offset) const {
        return KernelPtrAdapter(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p_) + offset));
    }

private:
    const void* p_;
};

// static
zx_status_t MessagePacket::Create(const void* data, uint32_t data_size,
                                  uint32_t num_handles,
                                  fbl::unique_ptr<MessagePacket>* msg) {
    return MessagePacket::CreateCommon(KernelPtrAdapter(data), data_size, num_handles, msg);
}

zx_status_t MessagePacket::CopyDataTo(user_out_ptr<void> buf) const {
    size_t rem = data_size_;
    for (auto iter = buffers_->cbegin(); iter != buffers_->cend(); ++iter) {
        const size_t len = fbl::min(rem, kPayloadSize);
        const zx_status_t status = buf.copy_array_to_user(iter->data, len);
        if (unlikely(status != ZX_OK)) {
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
}

MessagePacket::MessagePacket(BufferList* buffers, uint32_t data_size,
                             uint32_t num_handles, Handle** handles)
    : buffers_(buffers), handles_(handles), data_size_(data_size),
      // NewPacket ensures that num_handles fits in 16 bits.
      num_handles_(static_cast<uint16_t>(num_handles)), owns_handles_(false) {
}

zx_txid_t MessagePacket::get_txid() const {
    if (data_size_ < sizeof(zx_txid_t)) {
        return 0;
    }

    // GCC doesn't like it when we simply chain cast (dereferencing type-punned pointer will break
    // strict-aliasing rules) so go through an automatic variable.
    const void* p = reinterpret_cast<const void*>(buffers_->front().data);
    zx_txid_t txid = *reinterpret_cast<const zx_txid_t*>(p);
    return txid;
}
