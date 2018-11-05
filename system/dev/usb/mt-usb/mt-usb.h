// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <ddktl/device.h>
#include <ddktl/mmio.h>
#include <ddktl/pdev.h>
#include <ddktl/protocol/usb-dci.h>
#include <fbl/macros.h>
#include <fbl/mutex.h>
#include <fbl/optional.h>
#include <fbl/unique_ptr.h>
#include <lib/zx/handle.h>
#include <lib/zx/interrupt.h>
#include <zircon/listnode.h>
#include <zircon/hw/usb.h>

#include <threads.h>

namespace mt_usb {

class MtUsb;
using MtUsbType = ddk::Device<MtUsb, ddk::Unbindable>;

class MtUsb : public MtUsbType, public ddk::UsbDciProtocol<MtUsb> {
public:
    explicit MtUsb(zx_device_t* parent, pdev_protocol_t* pdev)
        : MtUsbType(parent), pdev_(pdev) {}

    static zx_status_t Create(zx_device_t* parent);

    // Device protocol implementation.
    void DdkUnbind();
    void DdkRelease();

    // USB DCI protocol implementation.
     void UsbDciRequestQueue(usb_request_t* req);
     zx_status_t UsbDciSetInterface(const usb_dci_interface_t* interface);
     zx_status_t UsbDciConfigEp(const usb_endpoint_descriptor_t* ep_desc, const
                                usb_ss_ep_comp_descriptor_t* ss_comp_desc);
     zx_status_t UsbDciDisableEp(uint8_t ep_address);
     zx_status_t UsbDciEpSetStall(uint8_t ep_address);
     zx_status_t UsbDciEpClearStall(uint8_t ep_address);
     zx_status_t UsbDciGetBti(zx_handle_t* out_bti);
     size_t UsbDciGetRequestSize();

private:
    DISALLOW_COPY_ASSIGN_AND_MOVE(MtUsb);

    enum Ep0State {
        // Waiting for next setup request.
        EP0_IDLE,
        // Reading data for setup request.
        EP0_READ,
        // Writing data for setup request.
        EP0_WRITE,
    };

    enum EpDirection {
        EP_OUT,
        EP_IN,
    };

    struct Endpoint {
        // Endpoint number to use when indexing into hardware registers.
        uint8_t ep_num;
        EpDirection direction;
        uint8_t address;

        bool enabled  __TA_GUARDED(lock) = false;
        uint16_t max_packet_size;

        // Requests waiting to be processed.
        list_node_t queued_reqs __TA_GUARDED(lock);
        // request currently being processed.
        usb_request_t* current_req __TA_GUARDED(lock) = nullptr;
        list_node_t complete_reqs __TA_GUARDED(lock);

        // Offset into current_req during read and write.
        size_t cur_offset;

        fbl::Mutex lock;
    };

    zx_status_t Init();
    void InitEndpoint(Endpoint* ep, uint8_t ep_num, EpDirection direction);
    void InitEndpoints();
    void InitPhy();
    int IrqThread();

    void HandleSuspend();
    void HandleReset();
    void HandleEp0();
    void HandleEndpointTxLocked(Endpoint* ep) __TA_REQUIRES(ep->lock);
    void HandleEndpointRxLocked(Endpoint* ep) __TA_REQUIRES(ep->lock);

    void FifoRead(uint8_t ep_index, void* buf, size_t buflen, size_t* actual);
    void FifoWrite(uint8_t ep_index, const void* buf, size_t length);
    void EpQueueNextLocked(Endpoint* ep) __TA_REQUIRES(ep->lock);
    void StartEndpoint(Endpoint* ep);
    void StartEndpoints();

    Endpoint* EndpointFromAddress(uint8_t addr);

    inline ddk::MmioBuffer* usb_mmio() {
        return &*usb_mmio_;
    }
    inline ddk::MmioBuffer* phy_mmio() {
        return &*phy_mmio_;
    }

    ddk::PDev pdev_;
    fbl::optional<ddk::UsbDciInterfaceProxy> dci_intf_;
    zx::bti bti_;

    fbl::optional<ddk::MmioBuffer> usb_mmio_;
    fbl::optional<ddk::MmioBuffer> phy_mmio_;

    zx::interrupt irq_;
    thrd_t irq_thread_;

    // Number of endpoints we support, not counting ep0.
    static constexpr size_t NUM_EPS = 15;

    Endpoint out_eps_[NUM_EPS];
    Endpoint in_eps_[NUM_EPS];

    // Address assigned to us by the host.
    uint8_t address_ = 0;
    bool set_address_ = false;

    // Current USB configuration. TODO this needs a lock.
    uint8_t configuration_ = 0;

    Ep0State ep0_state_ = EP0_IDLE;
    usb_setup_t cur_setup_;

    uint8_t ep0_data_[UINT16_MAX];
    // Current read/write location in ep0_buffer_
    size_t ep0_data_offset_ = 0;
    // Total length to read or write
    size_t ep0_data_length_ = 0;

    uint8_t ep0_max_packet_;
};

} // namespace mt_usb

__BEGIN_CDECLS
zx_status_t mt_usb_bind(void* ctx, zx_device_t* parent);
__END_CDECLS
