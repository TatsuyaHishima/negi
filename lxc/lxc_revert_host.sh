#!/bin/sh
CurrentDir=`pwd`
logtime=$1
machine_name=$2
lxc_name=$3

lxc-attach -n ${lxc_name} -o >&2 -- /root/lxc/lxc_revert_guest.sh "~/.negi/data/${logtime}" "${machine_name}"
