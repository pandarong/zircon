// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zx/vmo.h>
#include <fbl/mutex.h>
#include <fbl/vector.h>


namespace audio {
namespace utils {

class TdmVmoScatterList {
public:
  TdmVmoScatterList(zx_paddr_t buf, size_t length, size_t page_size);
  TdmVmoScatterList(zx_paddr_t buf, size_t length) {
      TdmVmoScatterList(buf, length, PAGE_SIZE);
  };

  zx_paddr_t GetPageStart(uint64_t offset);

private:
  struct MemRegion {
    zx_paddr_t start_paddr;
    size_t     length;
  };

  fbl::Vector<MemRegion> vmo_regions_;
};

} //namespace utils
} //namespace audio

