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
            p = subprocess.Popen(['ifconfig', ifEntry["ifDscr"], 'up'], stderr=subprocess.PIPE)
            output, err = p.communicate()
        else:
            p = subprocess.Popen(['ifconfig', ifEntry["ifDscr"], 'down'], stderr=subprocess.PIPE)
            output, err = p.communicate()

    if ifEntry["ifMtu"]:
        p = subprocess.Popen(['ifconfig', ifEntry["ifDscr"], 'mtu', str(ifEntry["ifMtu"])], stderr=subprocess.PIPE)
        output, err = p.communicate()

ipAddrEntries = tmp["ip"]["ipAddrTable"]["ipAddrEntry"]
for ipAddrEntry in ipAddrEntries:
    if "ipAdEntAddr" in ipAddrEntry:
        if re.search("^fe80", ipAddrEntry["ipAdEntAddr"]) or re.match("::1", ipAddrEntry["ipAdEntAddr"]):
            continue
        p = subprocess.Popen(['ifconfig', ipAddrEntry["ipAdEntIfIndex"], ipAddrEntry["ipAdEntAddr"], 'netmask', ipAddrEntry["ipAdEntNetMask"]], stderr=subprocess.PIPE)
        output, err = p.communicate()

ipRouteEntries = tmp["ip"]["ipRouteTable"]["ipRouteEntry"]
p = subprocess.Popen(['route', 'flush'], stderr=subprocess.PIPE)
output, err = p.communicate()

for ipRouteEntry in ipRouteEntries:
    if "ipRouteMask" in ipRouteEntry:
        p = subprocess.Popen(['route', 'add', '-net', ipRouteEntry["ipRouteDest"], '-netmask', ipRouteEntry["ipRouteMask"], ipRouteEntry["ipRouteNextHop"]], stderr=subprocess.PIPE)
        output, err = p.communicate()
    elif "ipRouteDest" in ipRouteEntry and "ipRouteNextHop" in ipRouteEntry:
        p = subprocess.Popen(['route', 'add', ipRouteEntry["ipRouteDest"], ipRouteEntry["ipRouteNextHop"]], stderr=subprocess.PIPE)
        output, err = p.communicate()
