#!/bin/bash -e

# oy

[ $# -eq 4 ] || exit 2
readonly OUTPUT="$1"
readonly INPUT="$2"
readonly DEPFILE="$3"
readonly PREFIX="$4"

manifest_files=()
input_files=()

read_file_list() {
  while read file; do
    if [ "${file%%/*}" = "$PREFIX" ]; then
      case "${file##*.}" in
        manifest)
          manifest_files+=("$file")
          ;;
        d)
          input_files+=($(<"$file"))
          ;;
        *)
          echo >&2 "unexpected runtime_deps file $file"
          exit 1
          ;;
      esac
    fi
  done
}

read_file_list < "$INPUT"

echo "$OUTPUT: ${manifest_files[*]} ${input_files[*]}" > "$DEPFILE"

LC_ALL=C sort "${manifest_files[@]}" > "$OUTPUT"
