#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/rtnetlink.h>
#include <net/if.h>

int main() {

	int soc;
	struct sockaddr_nl sa;
	struct {
		struct nlmsghdr nh;
		struct rtmsg rtm;/* hack */
	} request;

	int seq = 100;
	int n;
	char buf[4096];
	struct nlmsghdr *nlhdr;

	soc = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);

	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
 	sa.nl_pid = 0; /* kernel */
	sa.nl_groups = 0;

	request.nh.nlmsg_len = sizeof(request);
	request.nh.nlmsg_type = RTM_GETROUTE;/* hack */
	request.nh.nlmsg_flags = NLM_F_ROOT | NLM_F_REQUEST;
	request.nh.nlmsg_pid = 0;
	seq++;
	request.nh.nlmsg_seq = seq;
	request.rtm.rtm_family = AF_INET;

	n = sendto(soc, (void *)&request, sizeof(request), 0, (struct sockaddr *)&sa, sizeof(sa));
	if (n < 0) {
		perror("sendto");
		return 1;
	}

	printf("recv\n");

	n = recv(soc, buf, sizeof(buf), 0);
	if (n < 0) {
		perror("recvmsg");
		return 1;
	}

	for (nlhdr = (struct nlmsghdr *)buf; NLMSG_OK(nlhdr, n); nlhdr = NLMSG_NEXT(nlhdr, n)) {
		struct rtmsg *rtm;/* hack */
		struct rtattr *rta;
		int n;

		printf("====\n");
		// printf("len : %d\n", nlhdr->nlmsg_len);
		// printf("type : %d\n", nlhdr->nlmsg_type);

		if (nlhdr->nlmsg_type != RTM_NEWROUTE/* hack */) {
			printf("error : %d\n", nlhdr->nlmsg_type);
			continue;
		}

		rtm = NLMSG_DATA(nlhdr);

		printf(" family : %d\n", rtm->rtm_family);
		printf(" flags : %d\n", rtm->rtm_flags);
		printf(" scope : %d\n", rtm->rtm_scope);

		n = nlhdr->nlmsg_len - NLMSG_LENGTH(sizeof(struct rtmsg));

		for (rta = (struct rtattr *)RTM_RTA(rtm/* hack */); RTA_OK(rta, n); rta = RTA_NEXT(rta, n)) {
			printf(" type = %2d, len = %2d\t", rta->rta_type, rta->rta_len);

			switch (rta->rta_type) {
				/* hack */
				case RTN_UNSPEC:
				{
					printf("UNSPEC\n");
					break;				
				}
				case RTA_DST:/*	ゲートウェイまたはダイレクトな経路 */
				{
					printf("DST,RTN_UNICAST\n");
					char dst_addr[32];
					inet_ntop(AF_INET, RTA_DATA(rta), dst_addr, sizeof(dst_addr));
					printf("%s\n", dst_addr);
					break;
				}
				case RTA_SRC:	/* ローカルインターフェースの経路 */
				{
					printf("SRC,RTN_LOCAL\n");
					char src_addr[32];
					inet_ntop(AF_INET, RTA_DATA(rta), src_addr, sizeof(src_addr));
					printf("%s\n", src_addr);
					break;
				}
				case RTA_IIF:	/* ローカルなブロードキャスト経路 (ブロードキャストとして送信される) */
				{
					printf("IIF,RTN_BROADCAST\n");
					char buf[128];
					memset(buf, 0 , sizeof(buf));
					if_indextoname(*(int *)RTA_DATA(rta), buf);
					printf("%s\n", buf);
					break;
				}
				case RTA_OIF:	/* ローカルなブロードキャスト経路 (ユニキャストとして送信される) */
				{
					printf("OIF,RTN_ANYCAST\n");
					char buf[128];
					memset(buf, 0 , sizeof(buf));
					if_indextoname(*(int *)RTA_DATA(rta), buf);
					printf("%s\n", buf);
					break;
				}
				case RTN_MULTICAST:	/* マルチキャスト経路 */
				{
					printf("GATEWAY,RTN_MULTICAST\n");
					char gate_addr[32];
					inet_ntop(AF_INET, RTA_DATA(rta), gate_addr, sizeof(gate_addr));
					printf("%s\n", gate_addr);
					break;
				}
				case RTA_PRIORITY:	/* パケットを捨てる経路 */
				{
					printf("PRIORITY,RTN_BLACKHOLE\n");
					printf("%d\n", *(int *)RTA_DATA(rta));
					break;
				}
				case RTA_PREFSRC:	/* 到達できない行き先 */
				{
					printf("PREFSRC,RTN_UNREACHABLE\n");
					char prefsrc_addr[32];
					inet_ntop(AF_INET, RTA_DATA(rta), prefsrc_addr, sizeof(prefsrc_addr));
					printf("%s\n", prefsrc_addr);
					break;
				}
				case RTA_METRICS:	/* パケットを拒否する経路 */
				{
					printf("METRICS,RTN_PROHIBIT\n");
					printf("%d\n", *(int *)RTA_DATA(rta));
					break;
				}
				case RTA_MULTIPATH:	/* 経路探索を別のテーブルで継続 */
				{
					printf("MULTIPATH,RTN_THROW\n");
				// 	printf("%d\n", RTA_DATA(rta));
					break;
				}
				case RTA_PROTOINFO:	 /* ネットワークアドレスの変換ルール */
				{
					printf("PROTOINFO,RTN_NAT\n");
					// printf("%d\n", RTA_DATA(rta));
					break;
				}
				case RTA_FLOW:	/* 外部レゾルバを参照 (実装されていない) */
				{
					printf("FLOW,RTN_XRESOLVE\n");
					// printf("%d\n", RTA_DATA(rta));
					break;
				}
				case RTA_CACHEINFO:
				{
					printf("CACHEINFO\n");
					break;
				}
				default:
				printf("other type\n");
				break;
			}
		}
	}

	close(soc);

	return 0;
}