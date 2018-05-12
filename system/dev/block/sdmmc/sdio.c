// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ddk/device.h>
#include <ddk/debug.h>
#include <ddk/protocol/sdmmc.h>

#include "sdmmc.h"

zx_status_t sdio_io_rw_direct_host(sdmmc_device_t* dev, bool write, uint32_t fn, uint32_t addr, uint8_t in, uint8_t *out) {
      // TODO what is this parameter?
      uint32_t arg = write ? 0x80000000 : 0x00000000;
      arg |= fn << 28;
      arg |= addr << 9;
      arg |= (write && out) ? 0x08000000 : 0x00000000;
      arg |= in;

      sdmmc_req_t req = {
          .cmd_idx = SDIO_IO_RW_DIRECT,
          .arg = arg,
          .cmd_flags = SDIO_IO_RW_DIRECT_FLAGS,
      };
      zx_status_t st = sdmmc_request(&dev->host, &req);
      if (st != ZX_OK) {
          zxlogf(ERROR, "sd: SDIO_IO_RW_DIRECT failed, retcode = %d\n", st);
          return st;
      }
      if (out) {
          *out = req.response[0] & 0xFF;
          zxlogf(ERROR, "sd: SDIO_IO_RW_DIRECT GOT RESPONSE : 0x%x\n", *out);
      }
      return ZX_OK;
}

zx_status_t sdio_reset(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);

    //0
    if ((st = sdmmc_go_idle(dev)) != ZX_OK) {
        zxlogf(ERROR, "sdmmc: SDMMC_GO_IDLE_STATE failed, retcode = %d\n", st);
        device_remove(dev->zxdev);
        return st;
    }

    //8
    if ((st = sd_send_if_cond(dev)) != ZX_OK) {
        zxlogf(ERROR, "sdmmc: SD_SEND_IF_COND failed, retcode = %d\n", st);
        //device_remove(dev->zxdev);
        //return st;
    }

    //5
    uint32_t ocr;
    if ((st = sdio_send_op_cond(dev, 0, &ocr)) != ZX_OK) {
        zxlogf(ERROR, "mmc: MMC_SEND_OP_COND failed, retcode = %d\n", st);
        return st;
    }
    zxlogf(INFO, "sdmmc_probe_sdio OOHHOO Got OCR as :0x%x\n", ocr);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return ZX_OK;
}


zx_status_t sdio_reset1(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio1(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset2(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio2(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset3(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio3(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset4(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio4(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset5(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio5(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset6(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio6(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset7(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio7(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset8(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio8(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset9(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio9(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset10(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio10(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset11(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio11(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset12(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio12(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset13(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio13(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset14(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio14(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset15(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_rsdio1(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdioff_reset1(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sfdio1(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset1d(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_ddsdio1(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
zx_status_t sdio_reset17(sdmmc_device_t* dev) {
      zx_status_t status = ZX_OK;
      uint8_t abort;

      /* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */
      status = sdio_io_rw_direct_host(dev, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
      if (status != ZX_OK) {
          abort = 0x08;
      } else {
          abort |= 0x08;
      }
      status = sdio_io_rw_direct_host(dev, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
      zxlogf(INFO, "sdio_reset: Got status second time as %d\n", status);
      return status;
}

zx_status_t sdmmc_probe_sdio17(sdmmc_device_t* dev) {
    zx_status_t st = ZX_OK;
    dev->type = SDMMC_TYPE_SDIO;
    zxlogf(INFO, "sdmmc_probe_sdio HAHA Reached START\n");
    st = sdio_reset(dev);
    zxlogf(INFO, "sdmmc_probe_sdio COMPLETE Status:%d\n", st);
    return st;
}
