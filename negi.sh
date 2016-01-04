# !/bin/sh
CurrentDir="`pwd`"

function Usage() {
   echo "Usage"
   echo "./negi init"
   echo "       commit"
   echo "       revert"
   echo "       log [num]"
   echo "       machines {add, del, send, show}"
   echo "       config   {add, del, show}"
}

# 入力を受け付ける関数、defaultは$1で指定
function getInfo() {
   read text

   if [ ! -n "$text" ]; then
      text=$1
   fi
   echo $text
}

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
      expect \"\ \"
  	  send \"${command}\n\"
    } \"${id}@${host}'s password:\" {
      send \"${pass}\n\"
      expect \"\ \"
      send \"${command}\n\"
    }
    interact
  "
}

argv=("$@")
i=1

case ${argv[$i-1]} in

   init)
      # 本当に消去するかの確認
      if [ -d "${CurrentDir}/.negi" ]; then
         printf "Do you really delete exists file? [y/n] "
         answer="`getInfo "n"`"

         if [ ${answer} = "y" ]; then
            rm -rf "${CurrentDir}/.negi"
            echo "removed .negi"
         else
            exit
         fi
      fi

      mkdir "${CurrentDir}/.negi"
      echo "made .negi"

      touch "${CurrentDir}/.negi/config.csv"
      echo "# host_type, commit, revert, send" > ${CurrentDir}/.negi/config.csv
      echo "linux,linux_commit.sh,linux_revert.sh,linux_send.sh" >> ${CurrentDir}/.negi/config.csv
      echo "linux_vm,linux_vm_commit.sh,linux_vm_revert.sh,linux_vm_send.sh" >> ${CurrentDir}/.negi/config.csv
      echo "lxc,lxc_commit_host.sh,lxc_revert_host.sh,lxc_send.sh" >> ${CurrentDir}/.negi/config.csv

      touch "${CurrentDir}/.negi/machines.csv"
      echo "made .negi/machines.csv"

      printf "What is the host type? [Default: linux] "
      host_type=`getInfo "linux"`

      echo "# name, host_type, AddInfo1, AddInfo2, AddInfo3" > ${CurrentDir}/.negi/machines.csv
      echo "master,${host_type}" >> ${CurrentDir}/.negi/machines.csv

      mkdir "${CurrentDir}/.negi/data"
   ;;

   machines)
      i=`expr $i + 1`

      case ${argv[$i-1]} in
         add)

            printf "What is machine name? [Default: None] "
            name=`getInfo "None"`
            echo "ATTENSION: DO NOT CHANGE MACHINE NAME!"

            # 同一名は登録できない
            # machines.csvからデータの読み込み
            csvfile_machine="${CurrentDir}/.negi/machines.csv"
            for line in `cat "${csvfile_machine}" | grep -v ^#`
            do
               machine_name=`echo ${line} | cut -d ',' -f 1`
               if [ ${name} = ${machine_name} ]; then
                  echo "Can't set exists name."
                  exit
               fi
            done

            printf "What is the host type? [Default: linux_vm] "
            host_type=`getInfo "linux_vm"`

            printf "Add Info1? "
            add1=`getInfo ""`

            printf "Add Info2? "
            add2=`getInfo ""`

            printf "Add Info3? "
            add3=`getInfo ""`

            echo "${name},${host_type},${add1},${add2},${add3}" >> ${CurrentDir}/.negi/machines.csv
         ;;

         del)
            printf "What is machine name? "
            name=`getInfo ""`

            if [ ! -n "${name}" ]; then
               echo "Type machine name."
               exit
            fi

            # machines.csvからデータの読み込み
            csvfile_machine="${CurrentDir}/.negi/machines.csv"
            lineNum=1
            for line in `cat "${csvfile_machine}" | grep -v ^#`
            do
               lineNum=`expr $lineNum + 1`
               machine_name=`echo ${line} | cut -d ',' -f 1`
               if [ ${name} = ${machine_name} ]; then
                  sed -e "${lineNum}d" "${csvfile_machine}" > "${csvfile_machine}.tmp"
                  mv "${csvfile_machine}.tmp" "${csvfile_machine}"
                  exit
               fi
            done
            echo "There is no such device."

         ;;

         send)
         # machines.csvからデータの読み込み
         csvfile_machine="${CurrentDir}/.negi/machines.csv"
         for line in `cat "${csvfile_machine}" | grep -v ^#`
         do
            machine_name=`echo ${line} | cut -d ',' -f 1`
            machine_type=`echo ${line} | cut -d ',' -f 2`
            if [ ${machine_name} != "master" ]; then
              machine_add1=`echo ${line} | cut -d ',' -f 3`
              machine_add2=`echo ${line} | cut -d ',' -f 4`
              machine_add3=`echo ${line} | cut -d ',' -f 5`
              csvfile_config="${CurrentDir}/.negi/config.csv"
              for config_line in `cat "${csvfile_config}" | grep -v ^#`
              do
                 config_type=`echo ${config_line} | cut -d ',' -f 1`
                 if [ ${machine_type} = ${config_type} ]; then
                   config_send=`echo ${config_line} | cut -d ',' -f 4`
                   "${CurrentDir}/${config_type}/${config_send}" "${machine_add1}" "${machine_add2}" "${machine_add3}"
                 fi
              done
            fi
         done
         ;;

         *) # show
            # machines.csvからデータの読み込み
            csvfile_machine="${CurrentDir}/.negi/machines.csv"
            printf "name,\ttype,\tAddInfo1,\tAddInfo2,\tAddInfo3\n"
            for line in `cat "${csvfile_machine}" | grep -v ^#`
            do
               machine_name=`echo ${line} | cut -d ',' -f 1`
               machine_type=`echo ${line} | cut -d ',' -f 2`
               machine_add1=`echo ${line} | cut -d ',' -f 3`
               machine_add2=`echo ${line} | cut -d ',' -f 4`
               machine_add3=`echo ${line} | cut -d ',' -f 5`

               printf "${machine_name},\t${machine_type},\t${machine_add1},\t${machine_add2},\t${machine_add3}\n"
            done
         ;;
      esac
   ;;

   commit)
      commit_time=`date +"%Y%m%d%I%M%S"`
      commit_file_path="${CurrentDir}/.negi/data/${commit_time}"

      if [ -d "${commit_file_path}" ]; then
         echo "Don't commit twice in a second."
         exit
      fi

      mkdir "${commit_file_path}"
      touch "${commit_file_path}/message.txt"

      printf "Message? [Default: No message] "
      commit_message=`getInfo "No message"`

      echo "${commit_message}" >> "${commit_file_path}/message.txt"

      # machines.csvからデータの読み込み
      csvfile_machine="${CurrentDir}/.negi/machines.csv"
      for line in `cat "${csvfile_machine}" | grep -v ^#`
      do
         machine_name=`echo ${line} | cut -d ',' -f 1`
         machine_type=`echo ${line} | cut -d ',' -f 2`
         machine_add1=`echo ${line} | cut -d ',' -f 3`
         machine_add2=`echo ${line} | cut -d ',' -f 4`
         machine_add3=`echo ${line} | cut -d ',' -f 5`

         csvfile_config="${CurrentDir}/.negi/config.csv"
         for config_line in `cat "${csvfile_config}" | grep -v ^#`
         do
            config_type=`echo ${config_line} | cut -d ',' -f 1`
            config_commit=`echo ${config_line} | cut -d ',' -f 2`
            if [ ${config_type} = ${machine_type} ]; then
              "${CurrentDir}/${machine_type}/${config_commit}" "${commit_time}" "${machine_name}" "${machine_add1}" "${machine_add2}" "${machine_add3}"
            fi
         done
      done
   ;;

   config)
      i=`expr $i + 1`

      case ${argv[$i-1]} in
         add)
            printf "What is host type? "
            host_type=`getInfo ""`

            printf "What is the commit script name? "
            commit=`getInfo ""`

            printf "What is the revert script name? "
            revert=`getInfo ""`

            printf "What is the send script name? "
            send=`getInfo ""`

            echo "${host_type},${commit},${revert},${send}" >> "${CurrentDir}/.negi/config.csv"
         ;;
         del)
            printf "What is host type? "
            host_type=`getInfo ""`

            if [ ! -n "${host_type}" ]; then
               echo "Type OS name."
               exit
            fi

            # config.csvからデータの読み込み
            csvfile_config="${CurrentDir}/.negi/config.csv"
            lineNum=1
            for line in `cat "${csvfile_config}" | grep -v ^#`
            do
               lineNum=`expr $lineNum + 1`
               config_type=`echo ${line} | cut -d ',' -f 1`
               if [ ${config_type} = ${host_type} ]; then
                  sed -e "${lineNum}d" "${csvfile_config}" > "${csvfile_config}.tmp"
                  mv "${csvfile_config}.tmp" "${csvfile_config}"
                  exit
               fi
            done
            echo "There is no such OS."
         ;;

         *)
            # config.csvからデータの読み込み
            csvfile_config="${CurrentDir}/.negi/config.csv"
            printf "type,\tcommit,\trevert,\tsend\n"
            for line in `cat "${csvfile_config}" | grep -v ^#`
            do
               host_type=`echo ${line} | cut -d ',' -f 1`
               commit=`echo ${line} | cut -d ',' -f 2`
               revert=`echo ${line} | cut -d ',' -f 3`
               send=`echo ${line} | cut -d ',' -f 4`

               printf "${host_type},\t${commit},\t${revert},\t${send}\n"
            done
         ;;
      esac

   ;;

   revert)
      printf "Type the time. [Default: None] "
      logtime=`getInfo "None"`

      if [ "${logtime}" = "None" ]; then
         echo "Type log time."
         exit
      fi

      if [ ! -d "${CurrentDir}/.negi/data/${logtime}" ]; then
         echo "No such file."
         exit
      fi

      # machines.csvからデータの読み込み
      csvfile_machine="${CurrentDir}/.negi/machines.csv"
      for line in `cat "${csvfile_machine}" | grep -v ^#`
      do
         machine_name=`echo ${line} | cut -d ',' -f 1`
         machine_type=`echo ${line} | cut -d ',' -f 2`
         machine_add1=`echo ${line} | cut -d ',' -f 3`
         machine_add2=`echo ${line} | cut -d ',' -f 4`
         machine_add3=`echo ${line} | cut -d ',' -f 5`

         csvfile_config="${CurrentDir}/.negi/config.csv"
         for config_line in `cat "${csvfile_config}" | grep -v ^#`
         do
            config_type=`echo ${config_line} | cut -d ',' -f 1`
            config_revert=`echo ${config_line} | cut -d ',' -f 3`

            if [ ${config_type} = ${machine_type} ]; then
              "${CurrentDir}/${machine_type}/${config_revert}" "${logtime}" "${machine_name}" "${machine_add1}" "${machine_add2}" "${machine_add3}"
            fi
         done
      done
   ;;

   log)
      # 表示するログの数を指定
      log_print_num=5
      if expr "$2" : '[0-9]*' > /dev/null ; then
         log_print_num=$2
      fi

      # message.txtを取得
      folder_list=`ls -lt "${CurrentDir}/.negi/data/" | tail -n +2 | head -n "${log_print_num}" | awk '{print $9}'`
      folder_list_ary=(`echo ${folder_list}`)

      # ログを表示
      printf "\n"
      for i in `seq 1 ${#folder_list_ary[@]}`
      do
         message_txt=`cat "${CurrentDir}/.negi/data/${folder_list_ary[$i-1]}/message.txt"`
         printf "Time: "
         echo "${folder_list_ary[$i-1]}"
         printf "Log message: "
         echo ${message_txt}
         printf "\n"
      done

   ;;

   *)
      Usage
   ;;
esac
