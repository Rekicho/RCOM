#!/bin/bash

/etc/init.d/networking restart
ifconfig eth0 172.16.30.254/24
ifconfig eth2 172.16.31.253/24
echo 1 > /proc/sys/net/ipv4/ip_forward
route add default gw 172.16.31.254
