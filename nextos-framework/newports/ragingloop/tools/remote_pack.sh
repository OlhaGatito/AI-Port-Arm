set -e
cd /tmp
tar -cf - rlshots | gzip -1 > /tmp/rlshots.tgz
ls -l /tmp/rlshots.tgz
