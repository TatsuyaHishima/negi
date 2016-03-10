#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "commit_interface.h"
#include "commit_address.h"
#include "commit_route.h"
#include "common.h"
#include <jansson.h>

json_t *forward()
{
    FILE *fp;
    char *fname = "/proc/sys/net/ipv4/ip_forward";
    char  s[100];
    json_t *forward_num;

    fp = fopen(fname, "r");
    if (fp == NULL) {
        perror("fopen");
        exit(2);
    }

    while (fgets(s, 100, fp) != NULL) {
        int n = strlen(s);
        s[n-1] = '\0';
        return json_string(s);
    }
}

int main(int argc, char **argv)
{
    argc--;
    argv++;

    if (!argc) {
        printf("Usage: negi commit [filepath] [machine name].\n");
        exit(2);
    }

    char *filepath = *argv;
    argc--;
    argv++;

    if (!argc) {
        printf("Usage: negi commit [filepath] [machine name].\n");
        exit(2);
    }

    char *machine_name = *argv;
    argc--;
    argv++;

    json_t *linux_json = json_object();

    json_t *interface_json = json_object();
    interface_json = make_interface_file();

    if (json_object_size(interface_json) == 0) {
        fprintf(stderr, "Error: can't make interface file");
    }

    json_t *ip_json = json_object();

    json_object_set_new(ip_json, "ipForwarding", forward());

    json_t *address_json = json_object();
    address_json = make_address_file();

    if (json_object_size(address_json) == 0) {
        fprintf(stderr, "Error: can't make address file");
    }
    json_object_set_new(ip_json, "ipAddrTable", address_json);

    json_t *route_json = json_object();
    route_json = make_route_file();

    if (json_object_size(route_json) == 0) {
        fprintf(stderr, "Error: can't make route file");
    }
    json_object_set_new(ip_json, "ipRouteTable", route_json);

    json_object_set_new(linux_json, "interfaces", interface_json);
    json_object_set_new(linux_json, "ip", ip_json);

    char buf[2048];
    sprintf(buf, "%s/%s.json", filepath, machine_name);
    make_json_file(buf, linux_json);

    return 0;
}
