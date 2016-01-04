CurrentDir=`pwd`

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
      expect \"${id}@${host}'s password:\"
      send \"${pass}\n\"
      expect \"\# \"
      send \"${command}\n\"
    } \"${id}@${host}'s password:\" {
      send \"${pass}\n\"
      expect \"\# \"
      send \"${command}\n\"
    }
    interact
  "
}

host=$1
pass=$2
expect -c "
  set timeout 10
  spawn scp -r \"${CurrentDir}/linux_vm/\" \"root@${host}:~/\"
  expect \"Are you sure you want to continue connecting (yes/no)?\" {
    send \"yes\n\"
    expect \":\"
    send \"${pass}\n\"
  } \":\" {
    send \"${pass}\n\"
  }
  expect {\"100%\" { exit 0 }}
"
machine_os="linux_vm"
config_make="make"
script=`cat <<-SHELL
cd ~/${machine_os};
${config_make};
exit;
SHELL`

formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh ${host} "root" ${pass} "${formatted_script}"
