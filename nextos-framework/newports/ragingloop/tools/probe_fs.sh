#!/bin/sh
echo "readlink: $(readlink -f /storage/roms)"
stat -f -c 'fstype=%T' /storage/roms/ports
df -h /storage/roms/ports | tail -1
ls -ld /storage/roms /storage/roms/ports
touch /storage/roms/ports/.rl_case_test
ls /storage/roms/ports | grep -i rl_case
rm -f /storage/roms/ports/.rl_case_test
echo FS_CHECK_DONE
