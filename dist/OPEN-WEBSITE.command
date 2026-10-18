#!/bin/bash
set -eu
cd "$(dirname "$0")"
exec python3 site_start.py
