#!/usr/bin/python

import re
import json

def main():
    array = []
    for line in exec_cmd('netstat -rn'):
        if re.search("^Routing", line):
            continue
        elif re.search("^Internet", line):
            continue
        elif re.search("^Destination", line):
            continue
        else:
            elem = line.split()
            elem[0] = re.sub(r'%.*?/', '/', elem[0])
            elem[0] = re.sub(r'%.*?$', '', elem[0])
            # print elem[0] # destination
            if re.match(r'^fe80::', elem[0]) or re.match(r'::1', elem[0]):
                continue
            elif re.search(r':', elem[0]):
                entry = {"ipRouteDest": elem[0], "ipRouteIfIndex": elem[3], "ipRouteNextHop": elem[1], "ipRouteProto": elem[2]}
                array.append(entry)
            else:
                cmd = 'route -v show %s' % elem[0]
                for line2 in exec_cmd(cmd):
                    continue
                elem2 = line2.split()
                if len(elem2) > 4:
                    entry = {"ipRouteDest": elem2[0], "ipRouteIfIndex": elem[3], "ipRouteNextHop": elem2[4], "ipRouteProto": elem[2], "ipRouteMask": elem2[2]}
                else:
                    entry = {"ipRouteDest": elem[0], "ipRouteIfIndex": elem[3], "ipRouteNextHop": elem2[0], "ipRouteProto": elem[2]}
                array.append(entry)

    json_data = {"ipRouteEntry":array}
    encode_json_data = json.dumps(json_data, indent=4)

    f = open('tmp.json', 'w')
    f.write(encode_json_data)
    f.close

def exec_cmd(cmd):
    from subprocess import Popen, PIPE

    p = Popen(cmd.split(' '), stdout=PIPE, stderr=PIPE)
    out, err = p.communicate()

    return [ s for s in out.split('\n') if s ]

if __name__ == '__main__':
    main()

