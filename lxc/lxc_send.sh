# !/bin/sh
CurrentDir=`pwd`
lxc_name=$1

rm -rf "/var/lib/lxc/${lxc_name}/rootfs/root/lxc"
cp -r "${CurrentDir}/lxc" "/var/lib/lxc/${lxc_name}/rootfs/root/lxc"
echo "sent scripts"

lxc-attach -n ${lxc_name} -o >&2 -- /root/lxc/lxc_make.sh
