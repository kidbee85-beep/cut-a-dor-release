#!/bin/bash
# Cut-A-Dor recovery launcher. GPL-3.0-only. Never run automatically.
set -eu
cd "$(dirname "$0")"
printf '\nCUT-A-DOR / ORIGINAL V15 RESCUE\nRead RECOVERY.txt before continuing. This is the last-resort Mac USB route.\n'
if [ "$(uname -s)" != Darwin ]; then printf 'This launcher requires macOS.\n'; exit 1; fi
if ! xcode-select -p >/dev/null 2>&1; then printf 'Install Apple Command Line Tools first, then run this again.\n'; exit 1; fi
if [ "$(shasum -a 256 Original-V15.fwsc | cut -d' ' -f1)" != db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a ]; then
 printf 'Original V15 fingerprint mismatch. Stopping.\n'; exit 1
fi
printf 'Set up dependencies and start the check? Type CHECK to continue: '
read -r cad_rescue_reply
if [ "$cad_rescue_reply" != CHECK ]; then printf 'Cancelled.\n'; exit 0; fi
if [ ! -x .rescue-env/bin/python ]; then /usr/bin/python3 -m venv .rescue-env; fi
.rescue-env/bin/python -m pip install --disable-pip-version-check pyusb libusb-package
printf '\nThe rescue tool will check the chip and save a flash backup before asking to restore V15.\nIt downloads a fingerprint-checked USB loader. Enter your Mac password only in this Terminal.\n'
exec sudo "$PWD/.rescue-env/bin/python" "$PWD/fm1_rescue.py" "$PWD/Original-V15.fwsc" --ask --wait 300
