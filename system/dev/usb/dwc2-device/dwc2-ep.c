// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dwc2.h"


void dwc_ep_queue(dwc_usb_t* dwc, unsigned ep_num, usb_request_t* req) {

}

zx_status_t dwc_ep_config(dwc_usb_t* dwc, usb_endpoint_descriptor_t* ep_desc,
                          usb_ss_ep_comp_descriptor_t* ss_comp_desc) {
return -1;
}

zx_status_t dwc_ep_disable(dwc_usb_t* dwc, uint8_t ep_addr) {
return -1;
}

zx_status_t dwc_ep_set_stall(dwc_usb_t* dwc, unsigned ep_num, bool stall) {
return -1;
}
