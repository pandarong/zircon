// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>
#include <fbl/macros.h>
#include <lib/zx/handle.h>

#include "dwc3-types.h"

namespace dwc3 {

class Dwc3Fifo {
public:
    zx_status_t Init(size_t buffer_size, zx::unowned_handle bti);
    void Release();

    dwc3_trb_t* Next(bool set_current);
    void ReadAndClearCurrentTrb(dwc3_trb_t* out_trb);

    inline void FlushTrb(dwc3_trb_t* trb) {
        io_buffer_cache_flush(&buffer_, (trb - first_) * sizeof(*trb), sizeof(*trb));
    }

    inline void FlushInvalidateTrb(dwc3_trb_t* trb) {
        io_buffer_cache_flush_invalidate(&buffer_, (trb - first_) * sizeof(*trb), sizeof(*trb));
    }

    inline zx_paddr_t GetTrbPhys(dwc3_trb_t* trb) {
        return io_buffer_phys(&buffer_) + (trb - first_) * sizeof(*trb);
    }

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(Dwc3Fifo);

    io_buffer_t buffer_;
    dwc3_trb_t* first_;     // first TRB in the fifo
    dwc3_trb_t* next_;      // next free TRB in the fifo
    dwc3_trb_t* current_;   // TRB for currently pending transaction
    dwc3_trb_t* last_;      // last TRB in the fifo (link TRB)
};

} // namespace dwc3
