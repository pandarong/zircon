// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* These are only used by the rawnand test code */
uint32_t aml_get_ecc_pagesize(aml_rawnand_t *rawnand, uint32_t ecc_mode);
bool aml_check_write_protect(aml_rawnand_t *rawnand);

/* 
 * These are used by the NAND entry functions. These should move to a 
 * switch table, at a later point.
 */
void aml_cmd_ctrl(aml_rawnand_t *rawnand, int cmd, unsigned int ctrl);
uint8_t aml_read_byte(aml_rawnand_t *rawnand);
zx_status_t aml_read_page_hwecc(aml_rawnand_t *rawnand, void *data,
                                void *oob, uint32_t nandpage,
                                int *ecc_correct, bool page0);
zx_status_t aml_write_page_hwecc(aml_rawnand_t *rawnand,
                                 void *data, void *oob,
                                 uint32_t nandpage, bool page0);
zx_status_t aml_erase_block(aml_rawnand_t *rawnand, uint32_t nandpage);












