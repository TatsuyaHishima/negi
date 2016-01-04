# !/bin/sh
commit_time=$1
machine_name=$2

mkdir -p ~/.negi/data/${commit_time}
"~/linux_commit" "~/.negi/data/${commit_time}" "${machine_name}"
