# !/bin/sh
CurrentDir=`pwd`
commit_time=$1
machine_name=$2
lxc_name=$3

lxc-attach -n ${lxc_name} -o >&2 -- /root/lxc/lxc_commit_guest.sh "~/.negi/data/${commit_time}" "${machine_name}"
