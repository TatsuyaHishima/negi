# !/bin/sh
CurrentDir=`pwd`
logtime=$1
machine_name=$2

"${CurrentDir}/linux/linux_revert" "${CurrentDir}/.negi/data/${logtime}" "${machine_name}"
