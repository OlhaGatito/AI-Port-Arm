if [ -f /tmp/rlsurv.done ]; then echo DONE; fi
n=$(ps -eo comm= | grep -cw ragingloop || true)
echo "running=$n shots=$(ls /tmp/rlsurv 2>/dev/null | wc -l)"
tail -4 /tmp/rlsurv.log
