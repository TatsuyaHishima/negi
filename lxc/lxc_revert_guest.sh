# !/bin/sh
logtime=$1
machine_name=$2

"/root/linux_revert" "/root/.negi/data/${logtime}" "${machine_name}"
