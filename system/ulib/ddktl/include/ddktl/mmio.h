// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <zircon/assert.h>
#include <fbl/macros.h>
#include <hw/arch_ops.h>
#include <lib/zx/vmo.h>
/*
    MmioBlock is used to hold a reference to a block of memory mapped I/O, intended
    to be used in platform device drivers.
*/
namespace ddk {

class MmioBlock {

public:
    friend class Pdev;

    DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(MmioBlock);
    DISALLOW_NEW;

    MmioBlock() : MmioBlock(nullptr, 0) {}

    // Allow assignment from an rvalue
    MmioBlock& operator=(MmioBlock&& other) {
        transfer(other);
        return *this;
    }

    void reset(MmioBlock&& other) {
        transfer(other);
    }

    MmioBlock release() {
        return MmioBlock(*this);
    }

    void Info() {
        printf("ptr = %lx\n", ptr_);
        printf("len = %lu\n", len_);
        printf("vmo = %x\n", vmo_.get());
    }

    template<typename T = uint32_t>
    T Read(zx_off_t offs) {
        ZX_DEBUG_ASSERT(offs + sizeof(T) < len_);
        ZX_DEBUG_ASSERT(ptr_);
        return *reinterpret_cast<T*>(ptr_ + offs);
    }

    template<typename T = uint32_t>
    T ReadMasked(T mask, zx_off_t offs) {
        return (Read<T>(offs) & mask);
    }

    template<typename T = uint32_t>
    void Write(T val, zx_off_t offs) {
        ZX_DEBUG_ASSERT(offs + sizeof(T) < len_);
        ZX_DEBUG_ASSERT(ptr_);
        *reinterpret_cast<T*>(ptr_ + offs) = val;
        hw_mb();
    }

    template<typename T = uint32_t>
    void SetBits(T mask, zx_off_t offs) {
        T val = Read<T>(offs);
        Write<T>(val | mask, offs);
    }

    template<typename T = uint32_t>
    void ClearBits(T mask, zx_off_t offs) {
        T val = Read<T>(offs);
        Write<T>(val & ~mask, offs);
    }

    bool isMapped() {
        return ((ptr_ != 0) && (len_ != 0));
    }
    void* get() {
        return reinterpret_cast<void*>(ptr_);
    }

    ~MmioBlock() {
        // If we have a valid pointer and length, unmap on the way out
        if (isMapped()) {
            zx_vmar_unmap(zx_vmar_root_self(), ptr_, len_);
        }
    }

    MmioBlock(MmioBlock&&) = default;

private:
    void transfer(MmioBlock& other) {
            len_ = other.len_;
            ptr_ = other.ptr_;
            vmo_.reset(other.vmo_.release());
            other.len_ = 0;
            other.ptr_ = 0;
    }

    MmioBlock(MmioBlock& other) {
        transfer(other);
    }

    MmioBlock(void* ptr, size_t len) :
            ptr_(reinterpret_cast<uintptr_t>(ptr)),
            len_(len) {}

    MmioBlock(void* ptr, size_t len, zx_handle_t vmo) :
            ptr_(reinterpret_cast<uintptr_t>(ptr)),
            len_(len),
            vmo_(vmo) {}

    uintptr_t ptr_;
    size_t len_;
    zx::vmo vmo_;
};

} //namespace ddk