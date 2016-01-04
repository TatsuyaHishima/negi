# !/bin/sh
CurrentDir=`pwd`
commit_time=$1
machine_name=$2

"${CurrentDir}/linux/linux_commit" "${CurrentDir}/.negi/data/${commit_time}" "${machine_name}"
