#!/usr/bin/python
import subprocess
import json
import re
import sys

argvs = sys.argv
filename = "%s/%s.json" % (argvs[1], argvs[2])
f = open(filename)
tmp = json.load(f, encoding='utf-8')

ifEntries = tmp["interfaces"]["ifTable"]["ifEntry"]
for ifEntry in ifEntries:
    if "ifOperStatus" in ifEntry:
        if ifEntry["ifOperStatus"] == 1:
            subprocess.call(['ifconfig', ifEntry["ifDscr"], 'up'], shell=False)
        else:
            subprocess.call(['ifconfig', ifEntry["ifDscr"], 'down'], shell=False)

    if ifEntry["ifMtu"]:
        subprocess.call(['ifconfig', ifEntry["ifDscr"], 'mtu', str(ifEntry["ifMtu"])], shell=False)

ipAddrEntries = tmp["ip"]["ipAddrTable"]["ipAddrEntry"]
for ipAddrEntry in ipAddrEntries:
    if "ipAdEntAddr" in ipAddrEntry:
        if re.search("^fe80", ipAddrEntry["ipAdEntAddr"]) or re.match("::1", ipAddrEntry["ipAdEntAddr"]):
            continue
        subprocess.call(['ifconfig', ipAddrEntry["ipAdEntIfIndex"], ipAddrEntry["ipAdEntAddr"], 'netmask', ipAddrEntry["ipAdEntNetMask"]])

ipRouteEntries = tmp["ip"]["ipRouteTable"]["ipRouteEntry"]
subprocess.call(['route', 'flush'])
for ipRouteEntry in ipRouteEntries:
    if "ipRouteMask" in ipRouteEntry:
        subprocess.call(['route', 'add', '-net', ipRouteEntry["ipRouteDest"], '-netmask', ipRouteEntry["ipRouteMask"], ipRouteEntry["ipRouteNextHop"]])
    elif "ipRouteDest" in ipRouteEntry and "ipRouteNextHop" in ipRouteEntry:
        subprocess.call(['route', 'add', ipRouteEntry["ipRouteDest"], ipRouteEntry["ipRouteNextHop"]])
