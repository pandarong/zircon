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
      # Weed out the duplicates.
      new=true
      for dep in "${deps[@]}"; do
        if [ "$dep" = "$file" ]; then
          new=false
        fi
      done
      $new || continue
      deps+=("$file")
      set -- $(cat "$file") ${1+"$@"}
      ;;
    --files)
      args+=("$arg")
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
for arg in "${args[@]}"; do
  echo "$arg"
done > "$OUTPUT"
