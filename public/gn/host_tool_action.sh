#!/bin/bash
#
# Copyright 2018 The Fuchsia Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

RSPFILE="$1"
OUTPUT="$2"
DEPFILE="$3"
ORIG_DEPFILE="$4"
shift 4

CMD=()
FILES=()

read_rspfile() {
  local word
  while read word; do
    if [ "$word" = -- ]; then
      break
    else
      FILES+=("$word")
    fi
  done
  while read word; do
    CMD+=("$word")
  done
}

process_args() {
  local arg rspfile
  while [ $# -gt 0 ]; do
    arg="$1"
    shift
    if [[ "$arg" == @* ]]; then
      rspfile="${arg#@}"
      FILES+=("$rspfile")
      while read arg; do
        CMD+=("$arg")
      done < "$rspfile"
    else
      CMD+=("$arg")
    fi
  done
}

read_rspfile < "$RSPFILE"
process_args "$@"

"${CMD[@]}"

if [ "$ORIG_DEPFILE" = - ]; then
  # The host_tool_action() has no user-specified depfile, so just generate
  # one that lists the files from the metadata rspfile.
  echo "$OUTPUT: ${FILES[*]}" > "$DEPFILE"
else
  # Append the file list to the user-generated depfile.
  echo "$(< "$ORIG_DEPFILE") ${FILES[*]}" > "$DEPFILE"
fi
