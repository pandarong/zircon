// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dwc2.h"

bool dwc_ep_write_packet(dwc_usb_t* dwc, int ep_num) {
    dwc_regs_t* regs = dwc->regs;
    dwc_endpoint_t* ep = &dwc->eps[ep_num];

	uint32_t len = ep->req_length - ep->req_offset;
	if (len > ep->max_packet_size)
		len = ep->max_packet_size;

	uint32_t dwords = (len + 3) >> 2;
    uint8_t *req_buffer = &ep->req_buffer[ep->req_offset];

	dwc_gnptxsts_t txstatus = regs->gnptxsts;
	while  (ep->req_offset < ep->req_length && txstatus.nptxqspcavail > 0 && txstatus.nptxfspcavail > dwords) {
zxlogf(LINFO, "ep_num %d nptxqspcavail %u nptxfspcavail %u dwords %u\n", ep->ep_num, txstatus.nptxqspcavail, txstatus.nptxfspcavail, dwords);

    	volatile uint32_t* fifo = DWC_REG_DATA_FIFO(regs, ep_num);
    
    	for (uint32_t i = 0; i < dwords; i++) {
    		uint32_t temp = *((uint32_t*)req_buffer);
//zxlogf(LINFO, "write %08x\n", temp);
    		*fifo = temp;
    		req_buffer += 4;
    	}
    
    	ep->req_offset += len;

	    len = ep->req_length - ep->req_offset;
		if (len > ep->max_packet_size)
			len = ep->max_packet_size;

	    dwords = (len + 3) >> 2;
		txstatus = regs->gnptxsts;
	}

    if (ep->req_offset < ep->req_length) {
        // enable txempty
	    zxlogf(LINFO, "turn on nptxfempty\n");
		regs->gintmsk.nptxfempty = 1;
		return true;
    } else {
        return false;
    }
}

void dwc_ep_start_transfer(dwc_usb_t* dwc, unsigned ep_num, bool is_in, size_t length) {
if (ep_num > 0) zxlogf(LINFO, "dwc_ep_start_transfer epnum %u is_in %d length %zu\n", ep_num, is_in, length);
    dwc_regs_t* regs = dwc->regs;
    dwc_endpoint_t* ep = &dwc->eps[ep_num];

	volatile dwc_depctl_t* depctl_reg;
	volatile dwc_deptsiz_t* deptsiz_reg;
	uint32_t ep_mps = ep->max_packet_size;

    ep->req_offset = 0;
    ep->req_length = length;

	if (is_in) {
		depctl_reg = &regs->depin[ep_num].diepctl;
		deptsiz_reg = &regs->depin[ep_num].dieptsiz;
	} else {
	    if (ep_num > 0) {
	        ep_num -= 16;
	    }
		depctl_reg = &regs->depout[ep_num].doepctl;
		deptsiz_reg = &regs->depout[ep_num].doeptsiz;
	}

    dwc_depctl_t depctl = *depctl_reg;
	dwc_deptsiz_t deptsiz = *deptsiz_reg;

	/* Zero Length Packet? */
	if (length == 0) {
		deptsiz.xfersize = is_in ? 0 : ep_mps;
		deptsiz.pktcnt = 1;
	} else {
		deptsiz.pktcnt = (length + (ep_mps - 1)) / ep_mps;
		if (is_in && length < ep_mps) {
			deptsiz.xfersize = length;
		}
		else {
			deptsiz.xfersize = length - ep->req_offset;
		}
	}
zxlogf(LINFO, "epnum %d is_in %d xfer_count %d xfer_len %d pktcnt %d xfersize %d\n",
        ep_num, is_in, ep->req_offset, ep->req_length, deptsiz.pktcnt, deptsiz.xfersize);

    *deptsiz_reg = deptsiz;

	/* EP enable */
	depctl.cnak = 1;
	depctl.epena = 1;

    *depctl_reg = depctl;

    if (is_in) {
        dwc_ep_write_packet(dwc, ep_num);
    }
}

void dwc_complete_ep(dwc_usb_t* dwc, uint32_t ep_num) {
    zxlogf(LINFO, "XXXXX dwc_complete_ep ep_num %u\n", ep_num);

    if (ep_num != 0) {
    	dwc_endpoint_t* ep = &dwc->eps[ep_num];
        usb_request_t* req = ep->current_req;

        if (req) {
            ep->current_req = NULL;
            usb_request_complete(req, ZX_OK, ep->req_offset);
        }

        ep->req_buffer = NULL;
        ep->req_offset = 0;
        ep->req_length = 0;
	}

/*
	u32 epnum = ep_num;
	if (ep_num) {
		if (!is_in)
			epnum = ep_num + 1;
	}
*/


/*
	if (is_in) {
		pcd->dwc_eps[epnum].req->actual = ep->xfer_len;
		deptsiz.d32 = dwc_read_reg32(DWC_REG_IN_EP_TSIZE(ep_num));
		if (deptsiz.b.xfersize == 0 && deptsiz.b.pktcnt == 0 &&
                    ep->xfer_count == ep->xfer_len) {
			ep->start_xfer_buff = 0;
			ep->xfer_buff = 0;
			ep->xfer_len = 0;
		}
		pcd->dwc_eps[epnum].req->status = 0;
	} else {
		deptsiz.d32 = dwc_read_reg32(DWC_REG_OUT_EP_TSIZE(ep_num));
		pcd->dwc_eps[epnum].req->actual = ep->xfer_count;
		ep->start_xfer_buff = 0;
		ep->xfer_buff = 0;
		ep->xfer_len = 0;
		pcd->dwc_eps[epnum].req->status = 0;
	}
*/
}

static void dwc_ep_queue_next_locked(dwc_usb_t* dwc, dwc_endpoint_t* ep) {
    usb_request_t* req;

    if (ep->current_req == NULL &&
        (req = list_remove_head_type(&ep->queued_reqs, usb_request_t, node)) != NULL) {
        ep->current_req = req;
        
        usb_request_mmap(req, (void **)&ep->req_buffer);
        ep->send_zlp = req->header.send_zlp && (req->header.length % ep->max_packet_size) == 0;

	    dwc_ep_start_transfer(dwc, ep->ep_num, DWC_EP_IS_IN(ep->ep_num), req->header.length);
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

    uint32_t bit = 1 << ep_num;

    if (enable) {
        regs->daint |= bit;
        regs->daintmsk |= bit;
    } else {
        regs->daintmsk &= ~bit;
    }

    mtx_unlock(&dwc->lock);
}

static void dwc_ep_set_config(dwc_usb_t* dwc, unsigned ep_num, bool enable) {
    zxlogf(TRACE, "dwc3_ep_set_config %u\n", ep_num);

    if (enable) {
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

    for (unsigned ep_num = 1; ep_num < countof(dwc->eps); ep_num++) {
        dwc_ep_end_transfers(dwc, ep_num, ZX_ERR_IO_NOT_PRESENT);
        dwc_ep_set_stall(dwc, ep_num, false);
    }
}

void dwc_start_eps(dwc_usb_t* dwc) {
    zxlogf(TRACE, "dwc3_start_eps\n");

    for (unsigned ep_num = 1; ep_num < countof(dwc->eps); ep_num++) {
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
zxlogf(LINFO, "dwc_ep_config address %02x ep_num %d\n", ep_desc->bEndpointAddress, ep_num);
    if (ep_num == 0) {
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
        depctl_ptr = &regs->depin[ep_num].diepctl;
    } else {
        depctl_ptr = &regs->depout[ep_num - 16].doepctl;
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
        regs->depin[ep_num].diepctl.usbactep = 0;
    } else {
        regs->depout[ep_num - 16].doepctl.usbactep = 0;
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
