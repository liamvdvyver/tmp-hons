#!/usr/bin/env sh

python3 scripts/parse.py "$1" "$2"
./planner -y ir.json
