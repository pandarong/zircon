// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#pragma once

#include <fbl/canary.h>
#include <fbl/ref_ptr.h>
#include <object/dispatcher.h>
#include <object/port_dispatcher.h>
#include <zircon/types.h>
#include <vm/page_source.h>

// Page source implementation that talks to a userspace pager service
class PagerSource : public PageSource , public PortAllocator,
                    public fbl::DoublyLinkedListable<fbl::RefPtr<PagerSource>> {
public:
    PagerSource(uint64_t page_source_id,
                PagerDispatcher* dispatcher, fbl::RefPtr<PortDispatcher> port, uint64_t key);
    virtual ~PagerSource();

    PortPacket* Alloc() override {
        DEBUG_ASSERT(false);
        return nullptr;
    }
    void Free(PortPacket* port_packet) override;

    virtual bool GetPage(uint64_t offset,
                         vm_page_t** const page_out, paddr_t* const pa_out) override {
        // Pagers cannot synchronusly fulfill requests.
        return false;
    }

    void GetPageAsync(page_request_t* request) override;
    void ClearAsyncRequest(page_request_t* request) override;
    void SwapRequest(page_request_t* old, page_request_t* new_req) override;
    void OnClose() override;
    void OnDetach() override;
    zx_status_t WaitOnEvent(event_t* event) override;

private:
    PagerDispatcher* const pager_;
    const fbl::RefPtr<PortDispatcher> port_;
    const uint64_t key_;

    // The PageSource and PortDispatcher at various times keep references to memory
    // owned by this class. Once the page source drops the reference (closed_) and
    // the port drops the reference (complete_sent_), this can be cleaned up.
    fbl::Mutex mtx_;
    bool closed_ TA_GUARDED(mtx_) = false;
    bool complete_pending_ TA_GUARDED(mtx_) = false;

    // PortPacket used for sending all page requests to the pager service. The pager
    // dispatcher serves as packet_'s allocator. This informs the dispatcher when
    // packet_ is freed by the port, which lets the single packet be continuously reused
    // for all of the source's page requests.
    PortPacket packet_ = PortPacket(nullptr, this);
    // Bool indicating whether or not packet_ is currently queued in the port.
    bool packet_busy_ TA_GUARDED(mtx_) = false;
    // The page_request_t which corresponds to the current packet_.
    page_request_t* active_request_ TA_GUARDED(mtx_) = nullptr;
    // Queue of page_request_t's that have come in while packet_ is busy. The
    // head of this queue is sent to the port when packet_ is freed.
    list_node_t pending_requests_ TA_GUARDED(mtx_) = LIST_INITIAL_VALUE(pending_requests_);

    // page_request_t struct used for the complete message.
    page_request_t complete_request_ TA_GUARDED(mtx_) = {
        .node = LIST_INITIAL_CLEARED_VALUE, .offset = 0, .length = 0,
    };

    // Queues the page request, either sending it to the port or putting it in pending_requests_.
    void QueueMessageLocked(page_request_t* request) TA_REQ(mtx_);

    // Called when the packet becomes free. If pending_requests_ is non-empty, queues the
    // next request.
    void OnPacketFreedLocked() TA_REQ(mtx_);

    friend PagerDispatcher;
};

class PagerDispatcher final : public SoloDispatcher<PagerDispatcher, ZX_DEFAULT_PAGER_RIGHTS> {
public:
    static zx_status_t Create(fbl::RefPtr<Dispatcher>* dispatcher, zx_rights_t* rights);
    ~PagerDispatcher() final;

    zx_status_t CreateSource(fbl::RefPtr<PortDispatcher> port,
                             uint64_t key, fbl::RefPtr<PageSource>* src);
    void ReleaseSource(PagerSource* src);

    zx_obj_type_t get_type() const final { return ZX_OBJ_TYPE_PAGER; }

    void on_zero_handles() final;

private:
    explicit PagerDispatcher();

    fbl::Canary<fbl::magic("PGRD")> canary_;

    fbl::Mutex mtx_;
    fbl::DoublyLinkedList<fbl::RefPtr<PagerSource>> srcs_;
};
