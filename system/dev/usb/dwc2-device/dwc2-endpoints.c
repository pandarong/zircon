// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dwc2.h"

void dwc_ep_start_transfer(dwc_usb_t* dwc, unsigned ep_num, bool is_in) {
printf("dwc_ep_start_transfer epnum %u is_in %d\n", ep_num, is_in);
    dwc_regs_t* regs = dwc->regs;
    dwc_endpoint_t* ep = &dwc->eps[ep_num];

	volatile dwc_depctl_t* depctl_reg;
	volatile dwc_deptsiz_t* deptsiz_reg;
	uint32_t ep_mps = ep->max_packet_size;
//    _ep->total_len = _ep->xfer_len;

	if (is_in) {
		depctl_reg = &regs->depin[ep_num].diepctl;
		deptsiz_reg = &regs->depin[ep_num].dieptsiz;
	} else {
		depctl_reg = &regs->depout[ep_num].doepctl;
		deptsiz_reg = &regs->depout[ep_num].doeptsiz;
	}

    dwc_depctl_t depctl = *depctl_reg;
	dwc_deptsiz_t deptsiz = *deptsiz_reg;

	/* Zero Length Packet? */
	if (ep->req_length == 0) {
		deptsiz.xfersize = is_in ? 0 : ep_mps;
		deptsiz.pktcnt = 1;
	} else {
		deptsiz.pktcnt = (ep->req_length + (ep_mps - 1)) / ep_mps;
		if (is_in && ep->req_length < ep_mps) {
			deptsiz.xfersize = ep->req_length;
		}
		else {
			deptsiz.xfersize = ep->req_length - ep->req_offset;
		}
	}

    *deptsiz_reg = deptsiz;

	/* IN endpoint */
	if (is_in) {
		/* First clear it from GINTSTS */
//?????		regs->gintsts.nptxfempty = 0;
		regs->gintmsk.nptxfempty = 1;
	}

	/* EP enable */
	depctl.cnak = 1;
	depctl.epena = 1;

    *depctl_reg = depctl;
}

static void dwc_ep_queue_next_locked(dwc_usb_t* dwc, dwc_endpoint_t* ep) {
    usb_request_t* req;

    if (ep->current_req == NULL &&
        (req = list_remove_head_type(&ep->queued_reqs, usb_request_t, node)) != NULL) {
        ep->current_req = req;
        
        usb_request_mmap(req, (void **)&ep->req_buffer);
        ep->req_offset = 0;
        ep->req_length = req->header.length;
        ep->send_zlp = req->header.send_zlp && (req->header.length % ep->max_packet_size) == 0;

	    dwc_ep_start_transfer(dwc, ep->ep_num, DWC_EP_IS_IN(ep->ep_num));
    }
}

static void dwc_ep_end_transfers(dwc_usb_t* dwc, unsigned ep_num, zx_status_t reason) {
    dwc_endpoint_t* ep = &dwc->eps[ep_num];
    mtx_lock(&ep->lock);

    if (ep->current_req) {
//        dwc_cmd_ep_end_transfer(dwc, ep_num);
        usb_request_complete(ep->current_req, reason, 0);
        ep->current_req = NULL;
    }

    usb_request_t* req;
    while ((req = list_remove_head_type(&ep->queued_reqs, usb_request_t, node)) != NULL) {
        usb_request_complete(req, reason, 0);
    }

    mtx_unlock(&ep->lock);
}

static void dwc_enable_ep(dwc_usb_t* dwc, unsigned ep_num, bool enable) {
    dwc_regs_t* regs = dwc->regs;

    mtx_lock(&dwc->lock);

    uint32_t daint = regs->daint;
    uint32_t bit = 1 << ep_num;

    if (enable) {
        daint |= bit;
    } else {
        daint &= ~bit;
    }
    regs->daint = daint;

    mtx_unlock(&dwc->lock);
}

static void dwc_ep_set_config(dwc_usb_t* dwc, unsigned ep_num, bool enable) {
    zxlogf(TRACE, "dwc3_ep_set_config %u\n", ep_num);

//    dwc_endpoint_t* ep = &dwc->eps[ep_num];

    if (enable) {
//        dwc3_cmd_ep_set_config(dwc, ep_num, ep->type, ep->max_packet_size, ep->interval, false);
//        dwc3_cmd_ep_transfer_config(dwc, ep_num);
        dwc_enable_ep(dwc, ep_num, true);
    } else {
        dwc_enable_ep(dwc, ep_num, false);
    }
}


void dwc_reset_configuration(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;

    mtx_lock(&dwc->lock);
    // disable all endpoints except EP0_OUT and EP0_IN
    regs->daint = 1;
    mtx_unlock(&dwc->lock);

    for (unsigned i = 2; i < countof(dwc->eps); i++) {
        dwc_ep_end_transfers(dwc, i, ZX_ERR_IO_NOT_PRESENT);
        dwc_ep_set_stall(dwc, i, false);
    }
}

void dwc_start_eps(dwc_usb_t* dwc) {
    zxlogf(TRACE, "dwc3_start_eps\n");

//    dwc3_cmd_ep_set_config(dwc, EP0_IN, USB_ENDPOINT_CONTROL, dwc->eps[EP0_IN].max_packet_size, 0,
//                           true);
//    dwc3_cmd_start_new_config(dwc, EP0_OUT, 2);

    for (unsigned ep_num = 2; ep_num < countof(dwc->eps); ep_num++) {
        dwc_endpoint_t* ep = &dwc->eps[ep_num];
        if (ep->enabled) {
            dwc_ep_set_config(dwc, ep_num, true);

            mtx_lock(&ep->lock);
            dwc_ep_queue_next_locked(dwc, ep);
            mtx_unlock(&ep->lock);
        }
    }
}

void dwc_ep_queue(dwc_usb_t* dwc, unsigned ep_num, usb_request_t* req) {
    dwc_endpoint_t* ep = &dwc->eps[ep_num];

    // OUT transactions must have length > 0 and multiple of max packet size
    if (DWC_EP_IS_OUT(ep_num)) {
        if (req->header.length == 0 || req->header.length % ep->max_packet_size != 0) {
            zxlogf(ERROR, "dwc_ep_queue: OUT transfers must be multiple of max packet size\n");
            usb_request_complete(req, ZX_ERR_INVALID_ARGS, 0);
            return;
        }
    }

    mtx_lock(&ep->lock);

    if (!ep->enabled) {
        mtx_unlock(&ep->lock);
        usb_request_complete(req, ZX_ERR_BAD_STATE, 0);
        return;
    }

    list_add_tail(&ep->queued_reqs, &req->node);

    if (dwc->configured) {
        dwc_ep_queue_next_locked(dwc, ep);
    }

    mtx_unlock(&ep->lock);
}

zx_status_t dwc_ep_config(dwc_usb_t* dwc, usb_endpoint_descriptor_t* ep_desc,
                          usb_ss_ep_comp_descriptor_t* ss_comp_desc) {
    dwc_regs_t* regs = dwc->regs;

    // convert address to index in range 0 - 31
    // low bit is IN/OUT
    unsigned ep_num = DWC_ADDR_TO_INDEX(ep_desc->bEndpointAddress);
    if (ep_num < 2) {
        // index 0 and 1 are for endpoint zero
        return ZX_ERR_INVALID_ARGS;
    }

    unsigned ep_type = usb_ep_type(ep_desc);
    if (ep_type == USB_ENDPOINT_ISOCHRONOUS) {
        zxlogf(ERROR, "dwc_ep_config: isochronous endpoints are not supported\n");
        return ZX_ERR_NOT_SUPPORTED;
    }

    dwc_endpoint_t* ep = &dwc->eps[ep_num];

    mtx_lock(&ep->lock);

    volatile dwc_depctl_t* depctl_ptr;

    if (DWC_EP_IS_IN(ep_num)) {
        depctl_ptr = &regs->depin[ep_num >> DWC_EP_IN_SHIFT].diepctl;
    } else {
        depctl_ptr = &regs->depout[ep_num >> DWC_EP_OUT_SHIFT].doepctl;
    }

    ep->max_packet_size = usb_ep_max_packet(ep_desc);
    ep->type = ep_type;
    ep->interval = ep_desc->bInterval;
    // TODO(voydanoff) USB3 support

    ep->enabled = true;

    dwc_depctl_t depctl = *depctl_ptr;

    depctl.mps = usb_ep_max_packet(ep_desc);
	depctl.eptype = usb_ep_type(ep_desc);
	depctl.setd0pid = 1;
	depctl.txfnum = 0;   //Non-Periodic TxFIFO
	depctl.usbactep = 1;

    depctl_ptr->val = depctl.val;

    dwc_enable_ep(dwc, ep_num, true);

    if (dwc->configured) {
        dwc_ep_queue_next_locked(dwc, ep);
    }

    mtx_unlock(&ep->lock);

    return ZX_OK;
}

zx_status_t dwc_ep_disable(dwc_usb_t* dwc, uint8_t ep_addr) {
    dwc_regs_t* regs = dwc->regs;

    // convert address to index in range 0 - 31
    // low bit is IN/OUT
    unsigned ep_num = DWC_ADDR_TO_INDEX(ep_addr);
    if (ep_num < 2) {
        // index 0 and 1 are for endpoint zero
        return ZX_ERR_INVALID_ARGS;
    }

    dwc_endpoint_t* ep = &dwc->eps[ep_num];
    mtx_lock(&ep->lock);

    if (DWC_EP_IS_IN(ep_num)) {
        regs->depin[ep_num >> DWC_EP_IN_SHIFT].diepctl.usbactep = 0;
    } else {
        regs->depout[ep_num >> DWC_EP_OUT_SHIFT].doepctl.usbactep = 0;
    }

    ep->enabled = false;
    mtx_unlock(&ep->lock);

    return ZX_OK;
}

zx_status_t dwc_ep_set_stall(dwc_usb_t* dwc, unsigned ep_num, bool stall) {
    if (ep_num >= countof(dwc->eps)) {
        return ZX_ERR_INVALID_ARGS;
    }

    dwc_endpoint_t* ep = &dwc->eps[ep_num];
    mtx_lock(&ep->lock);

    if (!ep->enabled) {
        mtx_unlock(&ep->lock);
        return ZX_ERR_BAD_STATE;
    }
/*
    if (stall && !ep->stalled) {
        dwc3_cmd_ep_set_stall(dwc, ep_num);
    } else if (!stall && ep->stalled) {
        dwc3_cmd_ep_clear_stall(dwc, ep_num);
    }
*/
    ep->stalled = stall;
    mtx_unlock(&ep->lock);

    return ZX_OK;
}
