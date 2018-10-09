#!/usr/bin/env bash
# Copyright 2017 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

get_confirmation() {
  echo -n "Press 'y' to confirm: "
  read CONFIRM
  if [[ "$CONFIRM" != "y" ]]; then
    echo "[flash-imx8q-sdcard] Aborted due to invalid confirmation"
    exit 1
  fi
}

if [[ $OSTYPE != "linux-gnu" ]]; then
  echo "[flash-imx8q-sdcard] Script is currently Linux-exclusive"
  exit 1
fi

SCRIPT_DIR=$( cd $( dirname "${BASH_SOURCE[0]}" ) && pwd)
ZIRCON_DIR="$SCRIPT_DIR/.."

echo $ZIRCON_DIR


lsblk
echo "Enter the name of a block device to format: "
echo "     This will probably be of the form 'sd[letter]', like 'sdc'"
echo -n ">  "
read DEVICE

# Ensure that device exists
echo -n "[flash-imx8q-sdcard] Checking that device exists: $DEVICE ..."
DEVICE_PATH="/dev/$DEVICE"
if [[ ! -e "$DEVICE_PATH" ]]; then
  echo " FAILED"
  echo "[flash-imx8q-sdcard] ERROR: This device does not exist: $DEVICE_PATH"
  exit 1
fi
echo " SUCCESS"

# Ensure that the device is a real block device
echo -n "[flash-imx8q-sdcard] Checking that device is a known block device..."
if [[ ! -e "/sys/block/$DEVICE" ]]; then
  echo " FAILED"
  echo "[flash-imx8q-sdcard] ERROR: /sys/block/$DEVICE does not exist."
  echo "                     Does $DEVICE refer to a partition?"
  exit 1
fi
echo " SUCCESS"

for i in `cat /proc/mounts | grep "${DEVICE}" | awk '{print $2}'`; do umount $i; done

# Confirm that the user knows what they are doing
sudo -v -p "[sudo] Enter password to confirm information about device: "
sudo sfdisk -l "$DEVICE_PATH"
echo "[flash-imx8q-sdcard] ABOUT TO COMPLETELY WIPE / FORMAT: $DEVICE_PATH"
get_confirmation
echo "[flash-imx8q-sdcard] ARE YOU 100% SURE? This will hurt if you get it wrong."
get_confirmation

echo writing zircon image to dev/${DEVICE}1
sudo dd if="$ZIRCON_DIR/build-arm64/imx8q-quadmax-mek-boot.img" of=/dev/${DEVICE}1 bs=1M
echo writing vmbeta to dev/${DEVICE}13
sudo dd if="$ZIRCON_DIR/build-arm64/imx8q-quadmax-mek-vbmeta.img" of=/dev/${DEVICE}13 bs=1M

echo " SUCCESS"
