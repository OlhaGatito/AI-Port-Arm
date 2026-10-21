#!/bin/sh
# deploycheck: what BuildID / sha256 is actually on the device right now?
G=/roms/ports/beastiebay
ls -la $G/beastiebay-nextos
sha256sum $G/beastiebay-nextos
echo DEPLOYCHECK_DONE
