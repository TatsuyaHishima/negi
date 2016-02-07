#!/bin/bash

function auto_ssh() {
  host=$1
  pass=$2
  command=$3

  expect -c "
    set timeout 10
    spawn ssh ${host}
    expect \"Are you sure you want to continue connecting (yes/no)?\" {
      send \"yes\n\"
      expect \"password:\"
      send \"${pass}\n\"
      expect \"\# \"
  	  send \"${command}\n\"
    } \"password:\" {
      send \"${pass}\n\"
      expect \"\# \"
      send \"${command}\n\"
    }
    interact
  "
}

SCRIPT1_TIME=2000 # 20sec

START=`date "+%M%S%2N"`
TIME=`date "+%M%S%2N"`


./negi log
sleep(5)

script=`cat <<-SHELL
  clear;
	ip addr show dev ens9;
  sleep(5);
	exit;
SHELL`
formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh "guest1" "negipkj" "${formatted_script}"

STOP=`expr $START + $SCRIPT1_TIME`
while [ $TIME -lt $STOP ]
do
  TIME=`date "+%M%S%2N"`
  echo $TIME
done

expect -c "
  set timeout 10
  spawn ./negi revert
  expect \"Type the time.\" {
    send \"20160201055645\n\"
  }
  interact
"


STOP=`expr $STOP + $SCRIPT1_TIME`
while [ $TIME -lt $STOP ]
do
  TIME=`date "+%M%S%2N"`
  echo $TIME
done

script=`cat <<-SHELL
  clear;
	ip addr show dev ens9;
  sleep(5);
	exit;
SHELL`
formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh "guest1" "negipkj" "${formatted_script}"

script=`cat <<-SHELL
  clear;
  ping 192.168.0.1 -c 4;
  sleep(3);
  clear;
  traceroute -nI 192.168.0.1
	exit;
SHELL`
formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh "guest5" "negipkj" "${formatted_script}"
