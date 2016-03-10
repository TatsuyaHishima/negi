# !/bin/sh
commit_time=$1
machine_name=$2

/bin/mkdir -p /root/.negi/data/${commit_time}
"/root/linux_commit" "/root/.negi/data/${commit_time}" "${machine_name}"
