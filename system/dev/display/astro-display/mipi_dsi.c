// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "astro-display.h"

static uint32_t mipi_dsi_generic_read(astro_display_t* display, uint32_t address)
{
    uint32_t data_out;

    if (address != DW_DSI_GEN_PLD_DATA) {
        DISP_ERROR(" Error Address : %x\n", address);
    }

    data_out = READ32_REG(MIPI_DSI, address);
    return data_out;
}


static uint32_t generic_if_wr(astro_display_t* display, uint32_t address, uint32_t data_in)
{
    if ((address != DW_DSI_GEN_HDR) &&
        (address != DW_DSI_GEN_PLD_DATA)) {
        DISP_ERROR(" Error Address : 0x%x\n", address);
    }

    DISP_INFO("address 0x%x = 0x%08x\n", address, data_in);

    WRITE32_REG(MIPI_DSI, address, data_in);

    return 0;
}


static void dsi_generic_write_short_packet(astro_display_t* display, struct dsi_cmd_request_s *req)
{
    unsigned int d_para[2];

    switch (req->data_type) {
    case DT_GEN_SHORT_WR_1:
        d_para[0] = (req->pld_count == 0) ?
            0 : (((unsigned int)req->payload[2]) & 0xff);
        d_para[1] = 0;
        break;
    case DT_GEN_SHORT_WR_2:
        d_para[0] = (req->pld_count == 0) ?
            0 : (((unsigned int)req->payload[2]) & 0xff);
        d_para[1] = (req->pld_count < 2) ?
            0 : (((unsigned int)req->payload[3]) & 0xff);
        break;
    case DT_GEN_SHORT_WR_0:
    default:
        d_para[0] = 0;
        d_para[1] = 0;
        break;
    }

    generic_if_wr(display, DW_DSI_GEN_HDR,
        ((d_para[1] << BIT_GEN_WC_MSBYTE)          |
        (d_para[0] << BIT_GEN_WC_LSBYTE)           |
        (((unsigned int)req->vc_id) << BIT_GEN_VC) |
        (((unsigned int)req->data_type) << BIT_GEN_DT)));
    if (req->req_ack == MIPI_DSI_DCS_REQ_ACK)
        wait_bta_ack(display);
    else if (req->req_ack == MIPI_DSI_DCS_NO_ACK)
        wait_cmd_fifo_empty(display);
}
