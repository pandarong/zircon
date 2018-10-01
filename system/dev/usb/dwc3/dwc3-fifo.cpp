// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dwc3-fifo.h"

#include <string.h>

namespace dwc3 {


zx_status_t Dwc3Fifo::Init(size_t buffer_size, zx::unowned_handle bti) {
    ZX_DEBUG_ASSERT(buffer_size <= PAGE_SIZE);
    auto status = io_buffer_init(&buffer_, bti->get(), buffer_size, IO_BUFFER_RW | IO_BUFFER_CONTIG);
    if (status != ZX_OK) {
        return status;
    }

    first_ = static_cast<dwc3_trb_t*>(io_buffer_virt(&buffer_));
    next_ = first_;
    current_ = nullptr;
    last_ = first_ + (buffer_size / sizeof(dwc3_trb_t)) - 1;

    // set up link TRB pointing back to the start of the fifo
    dwc3_trb_t* trb = last_;
    zx_paddr_t trb_phys = io_buffer_phys(&buffer_);
    trb->ptr_low = (uint32_t)trb_phys;
    trb->ptr_high = (uint32_t)(trb_phys >> 32);
    trb->status = 0;
    trb->control = TRB_TRBCTL_LINK | TRB_HWO;
    io_buffer_cache_flush(&buffer_, (trb - first_) * sizeof(*trb), sizeof(*trb));

    return ZX_OK;
}

void Dwc3Fifo::Release() {
    io_buffer_release(&buffer_);
    first_ = next_ = current_ = last_ = nullptr;
}


dwc3_trb_t* Dwc3Fifo::Next(bool set_current) {
    dwc3_trb_t* trb = next_++;

    if (next_ == last_) {
        next_ = first_;
    }
    if (set_current && current_ == nullptr) {
        current_ = trb;
    }
    return trb;
}

void Dwc3Fifo::ReadAndClearCurrentTrb(dwc3_trb_t* out_trb) {
    ZX_DEBUG_ASSERT(current_ != nullptr);
    volatile dwc3_trb_t* trb = current_;
    current_ = nullptr;

    FlushInvalidateTrb(trb);
    out_trb->ptr_low = trb->ptr_low;
    out_trb->ptr_high = trb->ptr_high;
    out_trb->status = trb->status;
    out_trb->control = trb->control;
}

} // namespace dwc3
