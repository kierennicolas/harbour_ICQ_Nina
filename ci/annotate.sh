#!/bin/bash
# Publishes the tail of a log file as workflow annotations, in chunks, so the
# important part of a CI log can be read from the run page (and the API)
# without opening the full log.
# usage: ci/annotate.sh <error|warning|notice> <title> <logfile> [lines]
level=$1 title=$2 file=$3 lines=${4:-120}
[ -f "$file" ] || exit 0
mapfile -t L < <(tail -n "$lines" "$file" | sed -e 's/\x1b\[[0-9;]*[A-Za-z]//g' -e 's/\r$//')
part=1
for ((i = 0; i < ${#L[@]}; i += 40)); do
  msg=$(printf '%s\n' "${L[@]:i:40}" | sed -e 's/%/%25/g' | sed ':a;N;$!ba;s/\n/%0A/g')
  echo "::${level} title=${title} (${part})::${msg}"
  part=$((part + 1))
done
