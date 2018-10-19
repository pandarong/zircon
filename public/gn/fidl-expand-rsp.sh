#!/bin/sh -e

OUTPUT="$1"
shift
DEPFILE="$1"
shift

deps=()
args=()

while [ $# -gt 0 ]; do
  arg="$1"
  shift
  case "$arg" in
    @*)
      file="${arg#@}"
      deps+=("$file")
      set -- $(cat "$file") ${1+"$@"}
      ;;
    --*)
      args+=("$arg" "$1")
      shift
      ;;
    *)
      args+=("$arg")
      deps+=("$arg")
      ;;
  esac
done

echo "$OUTPUT:" "${deps[@]}" > "$DEPFILE"
echo "${args[@]}" > "$OUTPUT"
