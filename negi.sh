# !/bin/sh
CurrentDir="`pwd`"

function Usage() {
   echo "Usage"
}

# 入力を受け付ける関数、defaultは$1で指定
function getInfo() {
   read text

   if [ ! -n "$text" ]; then
      text=$1
   fi
   echo $text
}

argv=("$@")
i=1
# echo "argv[`expr $i - 1`]=${argv[$i-1]}"

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
      echo "# OS, commit, revert" > ${CurrentDir}/.negi/config.csv
      echo "linux,linux_negi,linux_negi" >> ${CurrentDir}/.negi/config.csv

      touch "${CurrentDir}/.negi/machines.csv"
      echo "made .negi/machines.csv"


      printf "Master machines name? [Default: master] "
      name=`getInfo "master"`

      printf "What is this OS? [Default: linux] "
      os=`getInfo "linux"`

      echo "# name, host_type, OS" > ${CurrentDir}/.negi/machines.csv
      echo "${name},master,${os}" >> ${CurrentDir}/.negi/machines.csv

      mkdir "${CurrentDir}/.negi/data"
   ;;

   machines)
      i=`expr $i + 1`

      case ${argv[$i-1]} in
         add)

            printf "What is machine name? [Default: None] "
            name=`getInfo "None"`
            echo "ATTENSION: DO NOT CHANGE MACHINE NAME!"

            # machines.csvからデータの読み込み
            csvfile="${CurrentDir}/.negi/machines.csv"
            for line in `cat "${csvfile}" | grep -v ^#`
            do
               machine_name=`echo ${line} | cut -d ',' -f 1`
               # 同一名は登録できない
               if [ ${name} = ${machine_name} ]; then
                  echo "Can't set exists name."
                  exit
               fi
            done

            printf "What is the type?(vm or container) [Default: vm] "
            type=`getInfo "vm"`

            printf "What is the OS? [Default: linux] "
            os=`getInfo "linux"`

            echo "${name},${type},${os}" >> ${CurrentDir}/.negi/machines.csv

         ;;

         del)
            printf "What is machine name? "
            name=`getInfo ""`

            if [ ! -n "${name}" ]; then
               echo "Type machine name."
               exit
            fi

            # machines.csvからデータの読み込み
            csvfile="${CurrentDir}/.negi/machines.csv"
            lineNum=1
            for line in `cat "${csvfile}" | grep -v ^#`
            do
               lineNum=`expr $lineNum + 1`
               machine_name=`echo ${line} | cut -d ',' -f 1`
               if [ ${name} = ${machine_name} ]; then
                  sed -e "${lineNum}d" "${csvfile}" > "${csvfile}.tmp"
                  mv "${csvfile}.tmp" "${csvfile}"
                  exit
               fi
            done
            echo "There is no such device."

         ;;

         *) # show
            # machines.csvからデータの読み込み
            csvfile="${CurrentDir}/.negi/machines.csv"
            printf "name\ttype\tOS\n"
            for line in `cat "${csvfile}" | grep -v ^#`
            do
               machine_name=`echo ${line} | cut -d ',' -f 1`
               machine_type=`echo ${line} | cut -d ',' -f 2`
               machine_os=`echo ${line} | cut -d ',' -f 3`

               printf "${machine_name}\t${machine_type}\t${machine_os}\n"
            done

         ;;
      esac
   ;;

   commit)

      commit_time=`date +"%Y%m%d%I%M%S"`
      commit_file_path="${CurrentDir}/.negi/data/${commit_time}"

      if [ -d "${commit_file_path}" ]; then
         echo "Don't commit twice in a seconds."
         exit
      fi

      mkdir "${commit_file_path}"
      touch "${commit_file_path}/message.txt"

      printf "Message? [Default: No message] "
      commit_message=`getInfo "No message"`

      echo ${commit_message} >> "${commit_file_path}/message.txt"

      # machines.csvからデータの読み込み
      csvfile="${CurrentDir}/.negi/machines.csv"
      for line in `cat "${csvfile}" | grep -v ^#`
      do
         machine_name=`echo ${line} | cut -d ',' -f 1`
         machine_type=`echo ${line} | cut -d ',' -f 2`
         machine_os=`echo ${line} | cut -d ',' -f 3`

        #  echo "name: ${machine_name}"
        #  echo "type: ${machine_type}"
        #  echo "os: ${machine_os}"
        #  echo "----行終わりっ！-------"
        #
         case ${machine_os} in
            linux)
               case ${machine_type} in
                  master)
                     "${CurrentDir}/negi_linux" commit "${commit_file_path}" "${machine_name}"
                  ;;
                  vm)
                  ;;
                  container)
                  ;;
                  *)
                  ;;
               esac
            ;;

            *)
               echo "skip ${machine_name}"
            ;;

         esac
      done


      # "${CurrentDir}/linux commit ${commit_file_path}"
   ;;

   config)
   i=`expr $i + 1`

   case ${argv[$i-1]} in
     add)
      printf "What is OS name? [Default: linux] "
      os=`getInfo "linux"`

      printf "What is the commit script name? [Default: negi_linux] "
      commit=`getInfo "negi_linux"`

      printf "What is the revert script name? [Default: negi_linux] "
      revert=`getInfo "negi_linux"`

      echo "${os},${commit},${revert}" >> "${CurrentDir}/.negi/config.csv"
     ;;
     del)
     printf "What is OS name?"
     os=`getInfo ""`

     if [ ! -n "${os}" ]; then
        echo "Type OS name."
        exit
     fi

     # config.csvからデータの読み込み
     csvfile="${CurrentDir}/.negi/config.csv"
     lineNum=1
     for line in `cat "${csvfile}" | grep -v ^#`
     do
        lineNum=`expr $lineNum + 1`
        os_name=`echo ${line} | cut -d ',' -f 1`
        if [ ${os} = ${os_name} ]; then
           sed -e "${lineNum}d" "${csvfile}" > "${csvfile}.tmp"
           mv "${csvfile}.tmp" "${csvfile}"
           exit
        fi
     done
     echo "There is no such OS."
     ;;

     *)
     # config.csvからデータの読み込み
     csvfile="${CurrentDir}/.negi/config.csv"
     printf "OS\tcommit\trevert\n"
     for line in `cat "${csvfile}" | grep -v ^#`
     do
        os=`echo ${line} | cut -d ',' -f 1`
        commit=`echo ${line} | cut -d ',' -f 2`
        revert=`echo ${line} | cut -d ',' -f 3`

        printf "${os}\t${commit}\t${revert}\n"
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
      csvfile="${CurrentDir}/.negi/machines.csv"
      for line in `cat "${csvfile}" | grep -v ^#`
      do
         machine_name=`echo ${line} | cut -d ',' -f 1`
         machine_type=`echo ${line} | cut -d ',' -f 2`
         machine_os=`echo ${line} | cut -d ',' -f 3`

        #  echo "name: ${machine_name}"
        #  echo "type: ${machine_type}"
        #  echo "os: ${machine_os}"
        #  echo "----行終わりっ！-------"
        #
         case ${machine_os} in
            linux)
               case ${machine_type} in
                  master)
                     "${CurrentDir}/negi_linux" revert "${CurrentDir}/.negi/data/${logtime}/${machine_name}.json"
                  ;;
                  vm)
                  ;;
                  container)
                  ;;
                  *)
                  ;;
               esac
            ;;

            *)
               echo "skip ${machine_name}"
            ;;

         esac
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

# i=`expr $i + 1` # 加算
# echo "argv[`expr $i - 1`]=${argv[$i-1]}"

# ./negi_linux commit
# ./negi_linux revert
