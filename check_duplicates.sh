#!/bin/bash

while read -r p; do
  f1=$(echo "$p" | cut -d' ' -f1)
  f2=$(echo "$p" | cut -d' ' -f2)

  h1=$(sha512sum "$f1" | cut -d' ' -f1)
  h2=$(sha512sum "$f2" | cut -d' ' -f1)

  if [ "$h1" != "$h2" ]; then
    echo "Files $f1 and $f2 were not duplicates"
  fi
done <duplicates.txt
