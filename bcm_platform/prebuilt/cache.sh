#!/system/bin/sh

while true; do echo 3 > /proc/sys/vm/drop_caches; sleep 3; done &
