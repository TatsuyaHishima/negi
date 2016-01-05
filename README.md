# negi
This is network environment generation management tool. Negi can now manage linux, linux_vm, lxc.

## Commands
- init
- commit
- revert
- log [num]
- machines
 - add
 - del
 - send
 - show
- config
 - add
 - del
 - show

## Install
Negi don't need to instal. Clone this repository.

### for CentOS7
Following is needed.

```
yum -y update
yum -y upgrade
```

```
yum -y install git make gcc bzip2
git clone https://github.com/TatsuyaHishima/negi.git
```

- jannson

```
curl -O http://www.digip.org/jansson/releases/jansson-2.7.tar.bz2

bunzip2 -c jansson-2.7.tar.bz2 | tar xf -
cd jansson-2.7
./configure
make
make check
make install # root needed
cd ../
```

- make script for master machine

```
vi /etc/ld.so.conf
#=> add "/usr/local/lib" to end of the line.
ldconfig
```
```
cd /path/to/negi/linux
make
```

### for Ubuntu14.04

```
apt-get -y update
apt-get -y upgrade
```

```
apt-get -y install git curl
```

- jannson

```
curl -O http://www.digip.org/jansson/releases/jansson-2.7.tar.bz2

bunzip2 -c jansson-2.7.tar.bz2 | tar xf -
cd jansson-2.7
./configure
make
make check
make install # root needed
cd ../
```

- make script for master machine

```
vi /etc/ld.so.conf
#=> add "/usr/local/lib" to end of the line.
ldconfig
```
```
cd /path/to/negi/linux
make
```

## Quick Usage
### 1\. init

```
./negi init
```

This command makes *.negi* directory and it will be used for managing generation like *.git*

### 2\. commit

```
./negi commit
```

This command saves network environment in *.negi* directory. 

The format of environment is written in *mib.json*. If you need to check what parameter is saved, check *mib.json*.

### 3\. revert

```
./negi revert
```

This command reverts network enviroment to commited environment.

You need commit time to revert. It is cleary shown in `./negi log` command.

4\. add machine

```
./negi machines add
```

This command adds machine under management of negi.

Each "host type" needs several addInfo, check below.

#### linux
This host type is for only host machine.
Usually you don't need to add this host type.

#### linux_vm
This host type is for virtual machine which OS is linux.

- addInfo1 - ip
- addInfo2 - root password

This script uses `ssh` command. So you can replace addInfo1(ip) to hostname if you've written *.ssh/config*

**ATTENTION** root password is managed in normal character.

#### lxc
This host type is for container of lxc.

- addInfo1 - container name

### 5\. send script to machines

```
./negi machines send
```

You need to send scripts(commit, revert, ...etc) to each machine after adding machine.

This script will do `make`, if it needed.

### 6\. for more host type...
You can add more host type on your own.

After coding commit script, revert script and send&make script, type

```
./negi config add
```

**If you make some script, please make pull request!**