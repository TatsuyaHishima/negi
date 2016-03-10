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

function wait_ms() {
  STOP=`expr $START + $1`
  while [ $TIME -lt $STOP ]
  do
    TIME=`date "+%M%S%2N"`
  done
}

START=`date "+%M%S%2N"`
TIME=`date "+%M%S%2N"`

#######################
echo "./negi log"
./negi log
sleep 8

script=`cat <<-SHELL
sleep 5;
clear;
ip addr show dev ens9;
exit;
SHELL`
formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh "guest1" "negipkj" "${formatted_script}"

wait_ms 2200
########################

expect -c "
  set timeout 10
  spawn ./negi revert
  expect \"Type the time.\" {
    send \"20160207103116\n\"
  }
  interact
"

wait_ms 4500
#########################

script=`cat <<-SHELL
sleep 5;
clear;
ip addr show dev ens9;
exit;
SHELL`
formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh "guest1" "negipkj" "${formatted_script}"

wait_ms 8500
#######################

script=`cat <<-SHELL
ip neigh flush dev ens9;
ip neigh flush dev ens10;
ip neigh flush dev ens11;
clear;
sleep 8;
traceroute -nI 192.168.0.1;
sleep 8;
ping 192.168.0.1 -c 4;
exit;
SHELL`
formatted_script=`echo "${script}" | sed -e 's/\ /\\\\ /g' | tr '\n' '\n'`
auto_ssh "guest5" "negipkj" "${formatted_script}"
