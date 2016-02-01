#!/system/bin/sh

sleep 5
PRIMARY_IP=$(ifconfig eth0 | busybox cut -c 10-22)
echo 1 > /proc/sys/net/ipv4/conf/eth0/promote_secondaries
echo 1 > /proc/sys/net/ipv4/conf/all/promote_secondaries
ip addr add 10.21.81.180/23 brd 10.21.81.255 dev eth0
ip link set eth0 up
ip addr del $PRIMARY_IP dev eth0
