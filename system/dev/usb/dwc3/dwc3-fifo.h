// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddk/io-buffer.h>

#include "dwc3-types.h"

namespace dwc3 {

class Dwc3Fifo {
public:

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(Dwc3Fifo);

    io_buffer_t buffer;
    dwc3_trb_t* first;      // first TRB in the fifo
    dwc3_trb_t* next;       // next free TRB in the fifo
    dwc3_trb_t* current;    // TRB for currently pending transaction
    dwc3_trb_t* last;       // last TRB in the fifo (link TRB)
};

} // namespace dwc3
