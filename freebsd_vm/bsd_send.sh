# !/bin/sh
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

host=$1
pass=$2
expect -c "
  set timeout 10
  spawn scp -r \"${CurrentDir}/freebsd_vm/\" \"root@${host}:~/\"
  expect \"Are you sure you want to continue connecting (yes/no)?\" {
    send \"yes\n\"
    expect \":\"
    send \"${pass}\n\"
  } \":\" {
    send \"${pass}\n\"
  }
  expect {\"100%\" { exit 0 }}
"
machine_os="freebsd_vm"
config_make="gcc addr.c -o addr -ljansson"
script=`cat <<-SHELL
cd ~/${machine_os};
${config_make};
mkdir -p ~/.negi/data/;
exit;
SHELL`

formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh ${host} "root" ${pass} "${formatted_script}"
