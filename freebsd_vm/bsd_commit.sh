# !/bin/sh
function auto_ssh() {
  host=$1
  id=$2
  pass=$3
  command=$4

  expect -c "
    set timeout 10
    spawn ssh ${id}@${host}
    expect \"Are you sure you want to continue connecting (yes/no)?\" {
      send \"yes\n\"
      expect \"Password for ${id}@${host}:\"
      send \"${pass}\n\"
      expect \"\# \"
  	  send \"${command}\n\"
    } \":\" {
      send \"${pass}\n\"
      expect \"\# \"
      send \"${command}\n\"
    }
    interact
  "
}

commit_time=$1
machine_name=$2
ip=$3
password=$4
addinfo3=$5

script=`cat <<-SHELL
  cd ~/;
  mkdir -p ~/.negi/data/${commit_time};
  python ~/freebsd_vm/netstat.py;
  ~/freebsd_vm/addr ~/.negi/data/${commit_time} ${machine_name};
  exit;
SHELL`

formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh ${ip} "root" ${password} "${formatted_script}"
