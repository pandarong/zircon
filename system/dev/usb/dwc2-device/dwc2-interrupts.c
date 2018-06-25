// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>

#include "dwc2.h"

#define DWC_REG_DATA_FIFO_START 0x1000
#define DWC_REG_DATA_FIFO(regs, ep)	((volatile uint32_t*)((uint8_t*)regs + (ep + 1) * 0x1000))

#define CLEAR_IN_EP_INTR(__epnum, __intr) \
do { \
        dwc_diepint_t diepint = {0}; \
	diepint.__intr = 1; \
	regs->depin[__epnum].diepint = diepint; \
} while (0)

#define CLEAR_OUT_EP_INTR(__epnum, __intr) \
do { \
        dwc_doepint_t doepint = {0}; \
	doepint.__intr = 1; \
	regs->depout[__epnum].doepint = doepint; \
} while (0)

static void dwc_ep_read_packet(dwc_regs_t* regs, void* buffer, uint32_t length, uint32_t ep_num) {
    uint32_t count = (length + 3) >> 2;
    uint32_t* dest = (uint32_t*)buffer;
	volatile uint32_t* fifo = DWC_REG_DATA_FIFO(regs, ep_num);

	for (uint32_t i = 0; i < count; i++) {
	    *dest++ = *fifo;
zxlogf(LSPEW, "read %08x\n", dest[-1]);
	}
}

static void dwc_ep_write_packet(dwc_usb_t* dwc, int epnum, uint32_t byte_count, uint32_t dword_count) {
if (epnum > 0) zxlogf(LINFO, "dwc_ep_write_packet ep %d byte_count: %u dword_count %u\n", epnum, byte_count, dword_count);
    dwc_regs_t* regs = dwc->regs;
    dwc_endpoint_t* ep = &dwc->eps[epnum];

	uint32_t i;
	volatile uint32_t* fifo;
	uint32_t temp_data;
    uint8_t *req_buffer = &ep->req_buffer[ep->req_offset];

	if (ep->req_offset >= ep->req_length) {
		zxlogf(LINFO, "dwc_ep_write_packet: No data for EP%d! offset %u length %u\n", epnum, ep->req_offset, ep->req_length);
		return;
	}

	fifo = DWC_REG_DATA_FIFO(regs, epnum);

	for (i = 0; i < dword_count; i++) {
		temp_data = *((uint32_t*)req_buffer);
zxlogf(LINFO, "write %08x\n", temp_data);
		*fifo = temp_data;
		req_buffer += 4;
	}

	ep->req_offset += byte_count;
}

static void dwc_set_address(dwc_usb_t* dwc, uint8_t address) {
    dwc_regs_t* regs = dwc->regs;
zxlogf(LINFO, "dwc_set_address %u\n", address);
    regs->dcfg.devaddr = address;
}

static void dwc2_ep0_out_start(dwc_usb_t* dwc)  {
//    zxlogf(LINFO, "dwc2_ep0_out_start\n");

    dwc_regs_t* regs = dwc->regs;

	dwc_deptsiz0_t doeptsize0 = {0};
	dwc_depctl_t doepctl = {0};

	doeptsize0.supcnt = 3;
	doeptsize0.pktcnt = 1;
	doeptsize0.xfersize = 8 * 3;
    regs->depout[0].doeptsiz.val = doeptsize0.val;

//??    dwc->ep0_state = EP0_STATE_IDLE;

	doepctl.epena = 1;
    regs->depout[0].doepctl = doepctl;
}

static void do_setup_status_phase(dwc_usb_t* dwc, bool is_in) {
//zxlogf(LINFO, "do_setup_status_phase is_in: %d\n", is_in);
//     dwc_endpoint_t* ep = &dwc->eps[0];

	dwc->ep0_state = EP0_STATE_STATUS;

	dwc_ep_start_transfer(dwc, 0, is_in, 0);

	/* Prepare for more SETUP Packets */
	dwc2_ep0_out_start(dwc);
}

static void dwc_ep0_complete_request(dwc_usb_t* dwc) {
     dwc_endpoint_t* ep = &dwc->eps[0];

    if (dwc->ep0_state == EP0_STATE_STATUS) {
//zxlogf(LINFO, "dwc_ep0_complete_request EP0_STATE_STATUS\n");
        ep->req_offset = 0;
        ep->req_length = 0;
// this interferes with zero length OUT
//    } else if ( ep->req_length == 0) {
//zxlogf(LINFO, "dwc_ep0_complete_request ep->req_length == 0\n");
//		dwc_otg_ep_start_transfer(ep);
    } else if (dwc->ep0_state == EP0_STATE_DATA_IN) {
//zxlogf(LINFO, "dwc_ep0_complete_request EP0_STATE_DATA_IN\n");
 	   if (ep->req_offset >= ep->req_length) {
	        do_setup_status_phase(dwc, false);
       }
    } else {
//zxlogf(LINFO, "dwc_ep0_complete_request ep0-OUT\n");
	    do_setup_status_phase(dwc, true);
    }

#if 0
	deptsiz0_data_t deptsiz;
	dwc_ep_t* ep = &pcd->dwc_eps[0].dwc_ep;
	int ret = 0;

	if (EP0_STATUS == pcd->ep0state) {
		ep->start_xfer_buff = 0;
		ep->xfer_buff = 0;
		ep->xfer_len = 0;
		ep->num = 0;
		ret = 1;
	} else if (0 == ep->xfer_len) {
		ep->xfer_len = 0;
		ep->xfer_count = 0;
		ep->sent_zlp = 1;
		ep->num = 0;
		dwc_otg_ep_start_transfer(ep);
		ret = 1;
	} else if (ep->is_in) {
		deptsiz.d32 = dwc_read_reg32(DWC_REG_IN_EP_TSIZE(0));
		if (0 == deptsiz.b.xfersize) {
			/* Is a Zero Len Packet needed? */
			do_setup_status_phase(pcd, 0);
		}
	} else {
		/* ep0-OUT */
		do_setup_status_phase(pcd, 1);
	}

#endif
}

static zx_status_t dwc_handle_setup(dwc_usb_t* dwc, usb_setup_t* setup, void* buffer, size_t length,
                                     size_t* out_actual) {
//zxlogf(LINFO, "dwc_handle_setup\n");
    zx_status_t status;
    dwc_endpoint_t* ep = &dwc->eps[0];

    if (setup->bmRequestType == (USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE)) {
        // handle some special setup requests in this driver
        switch (setup->bRequest) {
        case USB_REQ_SET_ADDRESS:
            zxlogf(INFO, "SET_ADDRESS %d\n", setup->wValue);
            dwc_set_address(dwc, setup->wValue);
            *out_actual = 0;
            return ZX_OK;
        case USB_REQ_SET_CONFIGURATION:
            zxlogf(INFO, "SET_CONFIGURATION %d\n", setup->wValue);
            dwc_reset_configuration(dwc);
            dwc->configured = false;
            status = usb_dci_control(&dwc->dci_intf, setup, buffer, length, out_actual);
            if (status == ZX_OK && setup->wValue) {
                dwc->configured = true;
                dwc_start_eps(dwc);
            }
            return status;
        default:
            // fall through to usb_dci_control()
            break;
        }
    } else if (setup->bmRequestType == (USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_INTERFACE) &&
               setup->bRequest == USB_REQ_SET_INTERFACE) {
        zxlogf(INFO, "SET_INTERFACE %d\n", setup->wValue);
        dwc_reset_configuration(dwc);
        dwc->configured = false;
        status = usb_dci_control(&dwc->dci_intf, setup, buffer, length, out_actual);
        if (status == ZX_OK) {
            dwc->configured = true;
            dwc_start_eps(dwc);
        }
        return status;
    }

    status = usb_dci_control(&dwc->dci_intf, setup, buffer, length, out_actual);
    if (status == ZX_OK) {
        ep->req_offset = 0;
        ep->req_length = *out_actual;
    }
    return status;
}

static void pcd_setup(dwc_usb_t* dwc) {
    usb_setup_t* setup = &dwc->cur_setup;

	if (!dwc->got_setup) {
//zxlogf(LINFO, "no setup\n");
		return;
	}
	dwc->got_setup = false;
//	_pcd->status = 0;


	if (setup->bmRequestType & USB_DIR_IN) {
//zxlogf(LINFO, "pcd_setup set EP0_STATE_DATA_IN\n");
		dwc->ep0_state = EP0_STATE_DATA_IN;
	} else {
//zxlogf(LINFO, "pcd_setup set EP0_STATE_DATA_OUT\n");
		dwc->ep0_state = EP0_STATE_DATA_OUT;
	}

    if (setup->wLength > 0 && dwc->ep0_state == EP0_STATE_DATA_OUT) {
//zxlogf(LINFO, "queue read\n");
        // queue a read for the data phase
        dwc->ep0_state = EP0_STATE_DATA_OUT;
        dwc_ep_start_transfer(dwc, 0, false, setup->wLength);
    } else {
        size_t actual;
        __UNUSED zx_status_t status = dwc_handle_setup(dwc, setup, dwc->ep0_buffer,
                                              sizeof(dwc->ep0_buffer), &actual);
        //zxlogf(INFO, "dwc_handle_setup returned %d actual %zu\n", status, actual);
//            if (status != ZX_OK) {
//                dwc3_cmd_ep_set_stall(dwc, EP0_OUT);
//                dwc3_queue_setup_locked(dwc);
//                break;
//            }

        if (dwc->ep0_state == EP0_STATE_DATA_IN && setup->wLength > 0) {
//            zxlogf(LINFO, "queue a write for the data phase\n");
            dwc->ep0_state = EP0_STATE_DATA_IN;
            dwc_ep_start_transfer(dwc, 0, true, actual);
        } else {
			dwc_ep0_complete_request(dwc);
        }
    }
}


static void dwc_handle_ep0(dwc_usb_t* dwc) {
//    zxlogf(LINFO, "dwc_handle_ep0\n");

	switch (dwc->ep0_state) {
	case EP0_STATE_IDLE: {
//zxlogf(LINFO, "dwc_handle_ep0 EP0_STATE_IDLE\n");
//		req_flag->request_config = 0;
		pcd_setup(dwc);
        break;
    }
	case EP0_STATE_DATA_IN:
//    zxlogf(LINFO, "dwc_handle_ep0 EP0_STATE_DATA_IN\n");
//		if (ep0->xfer_count < ep0->total_len)
//			zxlogf(LINFO, "FIX ME!! dwc_otg_ep0_continue_transfer!\n");
//		else
			dwc_ep0_complete_request(dwc);
		break;
	case EP0_STATE_DATA_OUT:
//    zxlogf(LINFO, "dwc_handle_ep0 EP0_STATE_DATA_OUT\n");
		dwc_ep0_complete_request(dwc);
		break;
	case EP0_STATE_STATUS:
//    zxlogf(LINFO, "dwc_handle_ep0 EP0_STATE_STATUS\n");
		dwc_ep0_complete_request(dwc);
		/* OUT for next SETUP */
		dwc->ep0_state = EP0_STATE_IDLE;
//		ep0->stopped = 1;
//		ep0->is_in = 0;
		break;

	case EP0_STATE_STALL:
	default:
		zxlogf(LINFO, "EP0 state is %d, should not get here pcd_setup()\n", dwc->ep0_state);
		break;
    }
}

void dwc_flush_fifo(dwc_usb_t* dwc, const int num) {
    dwc_regs_t* regs = dwc->regs;

    dwc_grstctl_t grstctl = {0};

	grstctl.txfflsh = 1;
	grstctl.txfnum = num;
	regs->grstctl = grstctl;
	
    uint32_t count = 0;
	do {
	    grstctl = regs->grstctl;
		if (++count > 10000)
			break;
	} while (grstctl.txfflsh == 1);

    zx_nanosleep(zx_deadline_after(ZX_USEC(1)));

	if (num == 0) {
		return;
    }

    grstctl.val = 0;
	grstctl.rxfflsh = 1;
	regs->grstctl = grstctl;

	count = 0;
	do {
	    grstctl = regs->grstctl;
		if (++count > 10000)
			break;
	} while (grstctl.rxfflsh == 1);

    zx_nanosleep(zx_deadline_after(ZX_USEC(1)));
}

static void dwc_handle_reset_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;

	zxlogf(LINFO, "\nUSB RESET\n");

    dwc->ep0_state = EP0_STATE_DISCONNECTED;

	/* Clear the Remote Wakeup Signalling */
	regs->dctl.rmtwkupsig = 1;

	for (int i = 0; i < MAX_EPS_CHANNELS; i++) {
	     dwc_depctl_t diepctl = regs->depin[i].diepctl;

        if (diepctl.epena) {
            // disable all active IN EPs
            diepctl.snak = 1;
            diepctl.epdis = 1;
    	    regs->depin[i].diepctl = diepctl;
        }

        regs->depout[i].doepctl.snak = 1;
	}

	/* Flush the NP Tx FIFO */
	dwc_flush_fifo(dwc, 0);

	/* Flush the Learning Queue */
	regs->grstctl.intknqflsh = 1;

    // EPO IN and OUT
	regs->daintmsk = (1 < DWC_EP_IN_SHIFT) | (1 < DWC_EP_OUT_SHIFT);

    dwc_doepint_t doepmsk = {0};
	doepmsk.setup = 1;
	doepmsk.xfercompl = 1;
	doepmsk.ahberr = 1;
	doepmsk.epdisabled = 1;
	regs->doepmsk = doepmsk;

    dwc_diepint_t diepmsk = {0};
	diepmsk.xfercompl = 1;
	diepmsk.timeout = 1;
	diepmsk.epdisabled = 1;
	diepmsk.ahberr = 1;
	regs->diepmsk = diepmsk;

	/* Reset Device Address */
	regs->dcfg.devaddr = 0;

	/* setup EP0 to receive SETUP packets */
	dwc2_ep0_out_start(dwc);

    // TODO how to detect disconnect?
    usb_dci_set_connected(&dwc->dci_intf, true);
}

static void dwc_handle_enumdone_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;

	zxlogf(INFO, "dwc_handle_enumdone_irq\n");


    if (dwc->astro_usb.ops) {
        astro_usb_do_usb_tuning(&dwc->astro_usb, false, false);
    }

    dwc->ep0_state = EP0_STATE_IDLE;

    dwc->eps[0].max_packet_size = 64;

    regs->depin[0].diepctl.mps = DWC_DEP0CTL_MPS_64;
    regs->depout[0].doepctl.epena = 1;

#if 0 // astro future use
	depctl.d32 = dwc_read_reg32(DWC_REG_IN_EP_REG(1));
	if (!depctl.b.usbactep) {
		depctl.b.mps = BULK_EP_MPS;
		depctl.b.eptype = 2;//BULK_STYLE
		depctl.b.setd0pid = 1;
		depctl.b.txfnum = 0;   //Non-Periodic TxFIFO
		depctl.b.usbactep = 1;
		dwc_write_reg32(DWC_REG_IN_EP_REG(1), depctl.d32);
	}

	depctl.d32 = dwc_read_reg32(DWC_REG_OUT_EP_REG(2));
	if (!depctl.b.usbactep) {
		depctl.b.mps = BULK_EP_MPS;
		depctl.b.eptype = 2;//BULK_STYLE
		depctl.b.setd0pid = 1;
		depctl.b.txfnum = 0;   //Non-Periodic TxFIFO
		depctl.b.usbactep = 1;
		dwc_write_reg32(DWC_REG_OUT_EP_REG(2), depctl.d32);
	}
#endif

    regs->dctl.cgnpinnak = 1;

	/* high speed */
#if 1 // astro
	regs->gusbcfg.usbtrdtim = 9;
#else
	regs->gusbcfg.usbtrdtim = 5;
#endif

    usb_dci_set_speed(&dwc->dci_intf, USB_SPEED_HIGH);
}

static void dwc_handle_rxstsqlvl_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;

//why?	regs->gintmsk.rxstsqlvl = 0;

	/* Get the Status from the top of the FIFO */
	 dwc_grxstsp_t grxstsp = regs->grxstsp;
zxlogf(LINFO, "dwc_handle_rxstsqlvl_irq epnum: %u bcnt: %u pktsts: %u\n", grxstsp.epnum, grxstsp.bcnt, grxstsp.pktsts);

    int ep_num = grxstsp.epnum;
    if (ep_num > 0) {
        ep_num += 16;
    }
    dwc_endpoint_t* ep = &dwc->eps[ep_num];

	switch (grxstsp.pktsts) {
	case DWC_STS_DATA_UPDT: {
	    uint32_t fifo_count = grxstsp.bcnt;
zxlogf(LINFO, "DWC_STS_DATA_UPDT grxstsp.bcnt: %u\n", grxstsp.bcnt);
        if (fifo_count > ep->req_length - ep->req_offset) {
zxlogf(LINFO, "fifo_count %u > %u\n", fifo_count, ep->req_length - ep->req_offset);
            fifo_count = ep->req_length - ep->req_offset;
        }
		if (fifo_count > 0) {
			dwc_ep_read_packet(regs, ep->req_buffer + ep->req_offset, fifo_count, ep_num);
			ep->req_offset += fifo_count;
		}
		break;
    }

	case DWC_DSTS_SETUP_UPDT: {
//zxlogf(LINFO, "DWC_DSTS_SETUP_UPDT\n"); 
    volatile uint32_t* fifo = (uint32_t *)((uint8_t *)regs + 0x1000);
    uint32_t* dest = (uint32_t*)&dwc->cur_setup;
    dest[0] = *fifo;
    dest[1] = *fifo;
zxlogf(LINFO, "SETUP bmRequestType: 0x%02x bRequest: %u wValue: %u wIndex: %u wLength: %u\n",
       dwc->cur_setup.bmRequestType, dwc->cur_setup.bRequest, dwc->cur_setup.wValue,
       dwc->cur_setup.wIndex, dwc->cur_setup.wLength); 
       dwc->got_setup = true;
		break;
	}

	case DWC_DSTS_GOUT_NAK:
zxlogf(LINFO, "DWC_DSTS_GOUT_NAK\n");
break;
	case DWC_STS_XFER_COMP:
//zxlogf(LINFO, "DWC_STS_XFER_COMP\n");
break;
	case DWC_DSTS_SETUP_COMP:
//zxlogf(LINFO, "DWC_DSTS_SETUP_COMP\n");
break;
	default:
		break;
	}
}

static void dwc_handle_inepintr_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;
	uint32_t ep_intr;
	uint32_t epnum = 0;

	/* Read in the device interrupt bits */
	ep_intr = regs->daint;
zxlogf(LINFO, "dwc_handle_inepintr_irq ep_intr %08x mask %08x\n", ep_intr, regs->daintmsk);
	ep_intr = (regs->daint & regs->daintmsk);
	ep_intr = (ep_intr & 0xffff);

	/* Clear all the interrupt bits for all IN endpoints in DAINT */
    regs->daint = 0xFFFF;

	/* Service the Device IN interrupts for each endpoint */
	while (ep_intr) {
		if (ep_intr & 0x1) {
		    dwc_diepint_t diepint = regs->depin[epnum].diepint;

			/* Transfer complete */
			if (diepint.xfercompl) {
if (epnum > 0) zxlogf(LINFO, "dwc_handle_inepintr_irq xfercompl\n");
				/* Disable the NP Tx FIFO Empty Interrrupt  */
//???		        regs->gintmsk.nptxfempty = 0;
				/* Clear the bit in DIEPINTn for this interrupt */
				CLEAR_IN_EP_INTR(epnum, xfercompl);
//				regs->depin[epnum].diepint.xfercompl = 1;
				/* Complete the transfer */
				if (0 == epnum) {
					dwc_handle_ep0(dwc);
				} else {
					dwc_complete_ep(dwc, epnum);
					if (diepint.nak) {
						CLEAR_IN_EP_INTR(epnum, nak);
				    }
				}
			}
			/* Endpoint disable  */
			if (diepint.epdisabled) {
				/* Clear the bit in DIEPINTn for this interrupt */
				CLEAR_IN_EP_INTR(epnum, epdisabled);
			}
			/* AHB Error */
			if (diepint.ahberr) {
				/* Clear the bit in DIEPINTn for this interrupt */
				CLEAR_IN_EP_INTR(epnum, ahberr);
			}
			/* TimeOUT Handshake (non-ISOC IN EPs) */
			if (diepint.timeout) {
//				handle_in_ep_timeout_intr(epnum);
zxlogf(LINFO, "TODO handle_in_ep_timeout_intr\n");
				CLEAR_IN_EP_INTR(epnum, timeout);
			}
			/** IN Token received with TxF Empty */
			if (diepint.intktxfemp) {
				CLEAR_IN_EP_INTR(epnum, intktxfemp);
			}
			/** IN Token Received with EP mismatch */
			if (diepint.intknepmis) {
				CLEAR_IN_EP_INTR(epnum, intknepmis);
			}
			/** IN Endpoint NAK Effective */
			if (diepint.inepnakeff) {
				CLEAR_IN_EP_INTR(epnum, inepnakeff);
			}
		}
		epnum++;
		ep_intr >>= 1;
	}
}

static void dwc_handle_outepintr_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;

//zxlogf(LINFO, "dwc_handle_outepintr_irq\n");

	uint32_t epnum = 0;

	/* Read in the device interrupt bits */
	uint32_t ep_intr = regs->daint & DWC_EP_OUT_MASK;
	ep_intr >>= DWC_EP_OUT_SHIFT;

	/* Clear the interrupt */
	regs->daint = DWC_EP_OUT_MASK;

	while (ep_intr) {
		if (ep_intr & 1) {
		    dwc_doepint_t doepint = regs->depout[epnum].doepint;
		    doepint.val &= regs->doepmsk.val;
if (epnum > 0) zxlogf(LINFO, "dwc_handle_outepintr_irq doepint.val %08x\n", doepint.val);

			/* Transfer complete */
			if (doepint.xfercompl) {
if (epnum > 0) zxlogf(LINFO, "dwc_handle_outepintr_irq xfercompl\n");
				/* Clear the bit in DOEPINTn for this interrupt */
				CLEAR_OUT_EP_INTR(epnum, xfercompl);

				if (epnum == 0) {
				    if (doepint.setup) { // astro
    					CLEAR_OUT_EP_INTR(epnum, setup);
    			    }
					dwc_handle_ep0(dwc);
				} else {
					dwc_complete_ep(dwc, epnum);
				}
			}
			/* Endpoint disable  */
			if (doepint.epdisabled) {
zxlogf(LINFO, "dwc_handle_outepintr_irq epdisabled\n");
				/* Clear the bit in DOEPINTn for this interrupt */
				CLEAR_OUT_EP_INTR(epnum, epdisabled);
			}
			/* AHB Error */
			if (doepint.ahberr) {
zxlogf(LINFO, "dwc_handle_outepintr_irq ahberr\n");
				CLEAR_OUT_EP_INTR(epnum, ahberr);
			}
			/* Setup Phase Done (contr0l EPs) */
			if (doepint.setup) {
			    if (1) { // astro
					dwc_handle_ep0(dwc);
				}
				CLEAR_OUT_EP_INTR(epnum, setup);
			}
		}
		epnum++;
		ep_intr >>= 1;
	}
}

static void dwc_handle_nptxfempty_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;

//zxlogf(LINFO, "dwc_handle_nptxfempty_irq\n");

	dwc_gnptxsts_t txstatus = {0};
	dwc_endpoint_t *ep = NULL;
	uint32_t len = 0;
	uint32_t dwords;
	uint32_t epnum = 0;

    /* Get the epnum from the IN Token Learning Queue. */
	for (epnum = 0; epnum < MAX_EPS_CHANNELS; epnum++) {
		ep = &dwc->eps[epnum];

		dwc_depctl_t depctl = regs->depin[epnum].diepctl;
		if (depctl.epena != 1) {
			continue;
        }

		/* While there is space in the queue and space in the FIFO and
		 * More data to tranfer, Write packets to the Tx FIFO */
		txstatus = regs->gnptxsts;
		while  (/*txstatus.b.nptxqspcavail > 0 &&
			txstatus.b.nptxfspcavail > dwords &&*/
		            ep->req_offset < ep->req_length) {
			uint32_t retry = 1000000;

			len = ep->req_length - ep->req_offset;
			if (len > ep->max_packet_size)
				len = ep->max_packet_size;

			dwords = (len + 3) >> 2;

			while (retry--) {
				txstatus = regs->gnptxsts;

zxlogf(LINFO, "ep_num %d nptxqspcavail %u nptxfspcavail %u dwords %u\n", ep->ep_num, txstatus.nptxqspcavail, txstatus.nptxfspcavail, dwords);

				if (txstatus.nptxqspcavail > 0 && txstatus.nptxfspcavail > dwords)
					break;
			}
			if (0 == retry) {
				zxlogf(LINFO, "TxFIFO FULL: Can't trans data to HOST !\n");
				break;
			}
			/* Write the FIFO */
			dwc_ep_write_packet(dwc, epnum, len, dwords);
		}
	}
}

static void dwc_handle_usbsuspend_irq(dwc_usb_t* dwc) {
    zxlogf(LINFO, "dwc_handle_usbsuspend_irq\n");
}


static void dwc_handle_irq(dwc_usb_t* dwc) {
    dwc_regs_t* regs = dwc->regs;
    dwc_interrupts_t interrupts = regs->gintsts;
    dwc_interrupts_t mask = regs->gintmsk;

    interrupts.val &= mask.val;

    if (!interrupts.val) {
        return;
    }

    // clear OTG interrupt
    uint32_t gotgint = regs->gotgint;
    regs->gotgint = gotgint;

zxlogf(LINFO, "dwc_handle_irq:");
if (interrupts.modemismatch) zxlogf(LINFO, " modemismatch");
if (interrupts.otgintr) zxlogf(LINFO, " otgintr");
if (interrupts.sof_intr) zxlogf(LINFO, " sof_intr");
if (interrupts.rxstsqlvl) zxlogf(LINFO, " rxstsqlvl");
if (interrupts.nptxfempty) zxlogf(LINFO, " nptxfempty");
if (interrupts.ginnakeff) zxlogf(LINFO, " ginnakeff");
if (interrupts.goutnakeff) zxlogf(LINFO, " goutnakeff");
if (interrupts.ulpickint) zxlogf(LINFO, " ulpickint");
if (interrupts.i2cintr) zxlogf(LINFO, " i2cintr");
if (interrupts.erlysuspend) zxlogf(LINFO, " erlysuspend");
if (interrupts.usbsuspend) zxlogf(LINFO, " usbsuspend");
if (interrupts.usbreset) zxlogf(LINFO, " usbreset");
if (interrupts.enumdone) zxlogf(LINFO, " enumdone");
if (interrupts.isooutdrop) zxlogf(LINFO, " isooutdrop");
if (interrupts.eopframe) zxlogf(LINFO, " eopframe");
if (interrupts.restoredone) zxlogf(LINFO, " restoredone");
if (interrupts.epmismatch) zxlogf(LINFO, " epmismatch");
if (interrupts.inepintr) zxlogf(LINFO, " inepintr");
if (interrupts.outepintr) zxlogf(LINFO, " outepintr");
if (interrupts.incomplisoin) zxlogf(LINFO, " incomplisoin");
if (interrupts.incomplisoout) zxlogf(LINFO, " incomplisoout");
if (interrupts.fetsusp) zxlogf(LINFO, " fetsusp");
if (interrupts.resetdet) zxlogf(LINFO, " resetdet");
if (interrupts.port_intr) zxlogf(LINFO, " port_intr");
if (interrupts.host_channel_intr) zxlogf(LINFO, " host_channel_intr");
if (interrupts.ptxfempty) zxlogf(LINFO, " ptxfempty");
if (interrupts.lpmtranrcvd) zxlogf(LINFO, " lpmtranrcvd");
if (interrupts.conidstschng) zxlogf(LINFO, " conidstschng");
if (interrupts.disconnect) zxlogf(LINFO, " disconnect");
if (interrupts.sessreqintr) zxlogf(LINFO, " sessreqintr");
if (interrupts.wkupintr) zxlogf(LINFO, " wkupintr");
zxlogf(LINFO, "\n");

    if (interrupts.usbreset) {
        dwc_handle_reset_irq(dwc);
    }
    if (interrupts.usbsuspend) {
        dwc_handle_usbsuspend_irq(dwc);
    }
    if (interrupts.enumdone) {
        dwc_handle_enumdone_irq(dwc);
    }
    if (interrupts.rxstsqlvl) {
        dwc_handle_rxstsqlvl_irq(dwc);
    }
    if (interrupts.inepintr) {
        dwc_handle_inepintr_irq(dwc);
    }
    if (interrupts.outepintr) {
        dwc_handle_outepintr_irq(dwc);
    }
    if (interrupts.nptxfempty) {
        dwc_handle_nptxfempty_irq(dwc);
    }

    regs->gintsts = interrupts;
}

// Thread to handle interrupts.
static int dwc_irq_thread(void* arg) {
    dwc_usb_t* dwc = (dwc_usb_t*)arg;

    while (1) {
/*        zx_status_t wait_res = zx_interrupt_wait(dwc->irq_handle, NULL);
        if (wait_res != ZX_OK) {
            zxlogf(ERROR, "dwc_usb: irq wait failed, retcode = %d\n", wait_res);
        }
*/
        dwc_handle_irq(dwc);
        usleep(10000);
    }

    zxlogf(INFO, "dwc_usb: irq thread finished\n");
    return 0;
}

zx_status_t dwc_irq_start(dwc_usb_t* dwc) {
    zx_status_t status = pdev_map_interrupt(&dwc->pdev, IRQ_INDEX, &dwc->irq_handle);
    if (status != ZX_OK) {
        return status;
    }
    thrd_create_with_name(&dwc->irq_thread, dwc_irq_thread, dwc, "dwc_irq_thread");
    return ZX_OK;
}

void dwc_irq_stop(dwc_usb_t* dwc) {
    zx_interrupt_destroy(dwc->irq_handle);
    thrd_join(dwc->irq_thread, NULL);
    zx_handle_close(dwc->irq_handle);
    dwc->irq_handle = ZX_HANDLE_INVALID;
}

