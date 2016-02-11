#include <stdio.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <err.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <jansson.h>

struct ifreq ifr;

int make_json_file(char *filename, json_t *json) {
	FILE *fp;

	fp = fopen( filename, "w" );
	if( fp == NULL ){
		printf( "%s couldn't open\n", filename );
		return -1;
	}

	// print json
	char *json_data = json_dumps(json, JSON_INDENT(4));
	sprintf(json_data, "%s\n", json_data); // add new line to end of file

	fputs(json_data, fp);

	fclose(fp);

	printf( "made \"%s\"\n", filename );
	return 0;
}

int
main(int argc, char **argv)
{
  struct ifaddrs *ifa_list;
  struct ifaddrs *ifa;
  int n, s;
  char addrstr[256], netmaskstr[256], broadstr[256], dststr[256];
  unsigned char *ptr;
  char buf[2048];
  int interface_flag;

  json_t *bsd_json = json_object();
  json_t *interfaces_json = json_object();
  json_t *ifTable_json = json_object();
  json_t *ifEntry_json_array = json_array();
  json_t *ip_json = json_object();
  json_t *ipAddrTable_json = json_object();
  json_t *ipAddrEntry_json_array = json_array();

  n = getifaddrs(&ifa_list);
  if (n != 0) {
    return 1;
  }

  for(ifa = ifa_list; ifa != NULL; ifa=ifa->ifa_next) {
  	interface_flag = 0;
  	json_t *interface_json = json_object();
  	json_t *address_json = json_object();

    if ((ifa->ifa_addr)->sa_family == AF_LINK) {
      interface_flag = 1;
      ptr = (unsigned char *)LLADDR((struct sockaddr_dl *)ifa->ifa_addr);
      sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", *ptr, *(ptr+1), *(ptr+2), *(ptr+3), *(ptr+4), *(ptr+5));
      json_object_set_new(interface_json, "ifPhysAddress", json_string((char *)buf));
    }

    json_object_set_new(interface_json, "ifDscr", json_string((char *)ifa->ifa_name));
    if (ifa->ifa_flags & IFF_UP)
      json_object_set_new(interface_json, "ifOperStatus", json_integer(1));
    else
      json_object_set_new(interface_json, "ifOperStatus", json_integer(2));

    // printf("ifname: %s\n", ifa->ifa_name);
    // printf("flags: %.8x\n", ifa->ifa_flags);

    memset(addrstr, 0, sizeof(addrstr));
    memset(netmaskstr, 0, sizeof(netmaskstr));

    ifr.ifr_addr.sa_family = AF_LOCAL;
    strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));

    s = socket(ifr.ifr_addr.sa_family, SOCK_DGRAM, 0);
    if (s < 0)
      err(1, "socket(family %u,SOCK_DGRAM)", ifr.ifr_addr.sa_family);

    if (ioctl(s, SIOCGIFINDEX, &ifr) != -1) {
      if (interface_flag) {
      	json_object_set_new(interface_json, "ifIndex", json_integer(ifr.ifr_index));
      } else {
      	json_object_set_new(address_json, "ipAdEntIfIndex", json_string((char *)ifa->ifa_name));
      }
      // printf("index: %d\n", ifr.ifr_index);
    }
    if (ioctl(s, SIOCGIFMETRIC, &ifr) != -1)
      // printf("metric: %d\n", ifr.ifr_metric);

    if (ioctl(s, SIOCGIFMTU, &ifr) != -1) {
      json_object_set_new(interface_json, "ifMtu", json_integer(ifr.ifr_mtu));
      // printf("mtu: %d\n", ifr.ifr_mtu);
    }

    if (ifa->ifa_addr->sa_family == AF_INET) {
      inet_ntop(AF_INET,
        &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
        addrstr, sizeof(addrstr));
      // printf("IP: %s\nnetmask: %s\n", addrstr, netmaskstr);
      json_object_set_new(address_json, "ipAdEntAddr", json_string((char *)addrstr));

      inet_ntop(AF_INET,
        &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr,
        netmaskstr, sizeof(netmaskstr));
      json_object_set_new(address_json, "ipAdEntNetMask", json_string((char *)netmaskstr));

      if (ioctl(s, SIOCGIFBRDADDR, &ifr) != -1) {
        inet_ntop(AF_INET,
          &((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_addr,
          broadstr, sizeof(broadstr));
        // sprintf(buf, "%s", broadstr);
        json_object_set_new(address_json, "ipAdEntBcastAddr", json_string((char *)broadstr));
      }

      if (ioctl(s, SIOCGIFDSTADDR, &ifr) != -1) {
        inet_ntop(AF_INET,
          &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
          dststr, sizeof(dststr));
        // printf("dst: %s\n", dststr);
        json_object_set_new(address_json, "ipAdEntDstAddr", json_string((char *)dststr));
      }

    } else if (ifa->ifa_addr->sa_family == AF_INET6) {
      inet_ntop(AF_INET6,
        &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr,
        addrstr, sizeof(addrstr));
      json_object_set_new(address_json, "ipAdEntAddr", json_string((char *)addrstr));

      inet_ntop(AF_INET6,
        &((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr,
        netmaskstr, sizeof(netmaskstr));
      json_object_set_new(address_json, "ipAdEntNetMask", json_string((char *)addrstr));
    }
    if (interface_flag) {
    	json_array_append(ifEntry_json_array, interface_json);
    } else {
    	json_array_append(ipAddrEntry_json_array, address_json);
    }
  }
  json_object_set_new(ifTable_json, "ifEntry", ifEntry_json_array);
  json_object_set_new(interfaces_json, "ifTable", ifTable_json);
  json_object_set_new(bsd_json, "interfaces", interfaces_json);
  json_object_set_new(ipAddrTable_json, "ipAddrEntry", ipAddrEntry_json_array);
  json_object_set_new(ip_json, "ipAddrTable", ipAddrTable_json);

  freeifaddrs(ifa_list);

  char *filename;
  filename = (char *)malloc(2048);
  sprintf(filename, "tmp.json");
  json_error_t error;
  json_t *tmp_json = json_load_file(filename, JSON_DECODE_ANY, &error);

  if (!tmp_json) {
    fprintf(stderr, "Error: can't read json file.\n");
    return -1;
  }
  json_object_set_new(ip_json, "ipRouteTable", tmp_json);
  json_object_set_new(bsd_json, "ip", ip_json);

  argc--;
  argv++;
  if (!argc) {
  	printf("type filepath.\n");
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

  char pathbuf[2048];
  sprintf(pathbuf, "%s/%s.json", filepath, machine_name);
  make_json_file(pathbuf, bsd_json);

  return 0;
}
