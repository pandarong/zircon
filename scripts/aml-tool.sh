#!/usr/bin/env bash

# Copyright 2017 The Fuchsia Authors
#
# Use of this source code is governed by a MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT

# -Creates boot image from ramdisk (bootdata.bin) and kdtb kernel image
# -Signs the created boot image
# -Flashes the target board (target board must be in update mode)

if [ "$#" -ne 2 ]; then
	echo "Usage: aml-tool.sh <amlogic vendor dir path> <zircon build path>"
	exit 1
fi

BUILD_DIR=$2

KDTB_FILE=$BUILD_DIR/zzircon.kdtb
OUT_IMAGE=$BUILD_DIR/zirconboot.img
RAMDISK=$BUILD_DIR/bootdata.bin

MEMBASE=0x00000000
KERNEL_OFFSET=0x01080000

CMDLINE="TERM=uart"

mkbootimg --kernel $KDTB_FILE \
--kernel_offset $KERNEL_OFFSET \
--base $MEMBASE \
--ramdisk $RAMDISK \
--cmdline $CMDLINE \
-o $OUT_IMAGE || exit 1

PRODUCT=biggie-p1

AML_PATH=$1

AML_USB_PATH=$AML_PATH/common/tools/aml-usb-boot/$PRODUCT
UPDATE_TOOL=$AML_USB_PATH/linux/update

SIGNING_TOOL_DIR=$AML_PATH/common/tools/signing/signing-tool-gxl
KEY_DIR=$AML_PATH/biggie/recovery/releasetools/keys

$SIGNING_TOOL_DIR/sign-boot-gxl.sh --sign-kernel -k $KEY_DIR/firmware_signk_0.pem -i $OUT_IMAGE -o $OUT_IMAGE.signed

#Init DDR
sudo $UPDATE_TOOL cwr $AML_USB_PATH/prebuilt/u-boot.bin.usb.bl2.signed 0xfffc0000
sudo $UPDATE_TOOL write $AML_USB_PATH/prebuilt/usbbl2runpara_ddrinit.bin 0xfffcc000
sudo $UPDATE_TOOL run 0xfffc0000

#Run u-boot
sudo $UPDATE_TOOL write $AML_USB_PATH/prebuilt/u-boot.bin.usb.bl2.signed 0xfffc0000
sudo $UPDATE_TOOL write $AML_USB_PATH/prebuilt/u-boot.bin.usb.tpl.signed 0x200c000
sudo $UPDATE_TOOL write $AML_USB_PATH/prebulit/usbbl2runpara_runfipimg.bin 0xfffcc000
sudo $UPDATE_TOOL run 0xfffc0000
sleep 5

sudo $UPDATE_TOOL partition boot $OUT_IMAGE.signed
