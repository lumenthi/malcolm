#include "../headers/malcolm.h"
#include "../headers/options.h"
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <stdlib.h>

static void	print_tcp_flags(struct tcphdr *header)
{
	int	count;
	int	first;

	count = 0;
	if (header->th_flags & TH_FIN)
		count++;
	if (header->th_flags & TH_SYN)
		count++;
	if (header->th_flags & TH_RST)
		count++;
	if (header->th_flags & TH_PUSH)
		count++;
	if (header->th_flags & TH_ACK)
		count++;
	if (header->th_flags & TH_URG)
		count++;
	first = 0;
	if (header->th_flags & TH_FIN) {
		if (first > 0)
			dprintf(STDOUT_FILENO, "|");
		dprintf(STDOUT_FILENO, "FIN");
		first++;
	}
	if (header->th_flags & TH_SYN) {
		if (first > 0)
			dprintf(STDOUT_FILENO, "|");
		dprintf(STDOUT_FILENO, "SYN");
		first++;
	}
	if (header->th_flags & TH_RST) {
		if (first > 0)
			dprintf(STDOUT_FILENO, "|");
		dprintf(STDOUT_FILENO, "RST");
		first++;
	}
	if (header->th_flags & TH_PUSH) {
		if (first > 0)
			dprintf(STDOUT_FILENO, "/");
		dprintf(STDOUT_FILENO, "PUSH");
		first++;
	}
	if (header->th_flags & TH_ACK) {
		if (first > 0)
			dprintf(STDOUT_FILENO, "|");
		dprintf(STDOUT_FILENO, "ACK");
		first++;
	}
	if (header->th_flags & TH_URG) {
		if (first > 0)
			dprintf(STDOUT_FILENO, "|");
		dprintf(STDOUT_FILENO, "URG");
		first++;
	}
}

static void print_icmp(struct icmphdr *icmp, struct iphdr *ip)
{
	dprintf(STDOUT_FILENO, "%s:", inet_ntoa(*(struct in_addr*)&ip->saddr));
	if (icmp->type == ICMP_ECHOREPLY)
		dprintf(STDOUT_FILENO, " ICMP Reply to ");
	else if (icmp->type == ICMP_ECHO)
		dprintf(STDOUT_FILENO, " ICMP Request to ");
	else
		dprintf(STDOUT_FILENO, " ICMP Code %d to ", icmp->code);	
	dprintf(STDOUT_FILENO, "%s\n", inet_ntoa(*(struct in_addr*)&ip->daddr));
}

static void	search_for_str(struct iphdr *ip, char *str, char *srch)
{
	char	*tmp;
	size_t	len;

	tmp = str;
	len = ft_strlen(srch);
	while ((tmp = ft_strstr(tmp, srch))) {
		size_t i = 0;
		while (i < len && *tmp) {
			i++;
			tmp++;
		}
		if (!*tmp)
			break;
		char *endl = ft_strchr(tmp, '\r');
		if (endl)
			*endl = 0;
		endl = ft_strchr(tmp, '\n');
		if (endl)
			*endl = 0;
		if (!ft_strcmp(srch, "Host: "))
			dprintf(STDOUT_FILENO, "%s HTTP request to ",
			inet_ntoa(*(struct in_addr*)&ip->saddr));
		dprintf(STDOUT_FILENO, "%s", tmp);
		fflush(stdout);
		if (!ft_strcmp(srch, "GET ") || !ft_strcmp(srch, "POST "))
			dprintf(STDOUT_FILENO, "\n");
		tmp++;
	}
}

static void	find_links(struct iphdr *ip, char *str)
{
	search_for_str(ip, str, "Host: ");
	search_for_str(ip, str, "GET ");
	search_for_str(ip, str, "Host: ");
	search_for_str(ip, str, "POST ");
}

static void print_tcp(struct tcphdr *tcp, struct iphdr *ip, ssize_t payload_size)
{
	/* Maybe for future uses */
	(void)print_tcp_flags;
	(void)tcp;
	if (payload_size > 0)
		find_links(ip, (char*)(tcp + 1));
}

static void print_udp(struct udphdr *udp, struct iphdr *ip)
{
	dprintf(STDOUT_FILENO, "%s:", inet_ntoa(*(struct in_addr*)&ip->saddr));
	dprintf(STDOUT_FILENO, "%d UDP", ft_ntohs(udp->uh_sport));
	dprintf(STDOUT_FILENO, " to %s:%d\n",
		inet_ntoa(*(struct in_addr*)&ip->daddr), ft_ntohs(udp->uh_dport));
}

static int transmit_packet(struct iphdr *ip, ssize_t size)
{
	int	l3fd;
	int one;
	struct sockaddr_in dst_addr;
	socklen_t addr_len = sizeof(struct sockaddr);

	l3fd = socket(AF_INET, SOCK_RAW, ip->protocol);

	if (l3fd < 0)
		fprintf(stderr, "[!] Failed to open layer 3 socket\n");

	one = 1;
	if ((setsockopt(l3fd, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one))) != 0) {
		fprintf(stderr, "[!] Failed to set header option for layer 3 socket\n");
		close(l3fd);
		return 1;
	}

	ft_bzero(&dst_addr, sizeof(dst_addr));
	dst_addr.sin_family = AF_INET;
	ft_memcpy(&dst_addr.sin_addr, &ip->daddr, sizeof(ip->daddr));

	ip->saddr = inet_addr("172.17.0.2");

	printf("[*] Transmitting packet\n");
	if (sendto(l3fd, ip, size, 0, (struct sockaddr*)&dst_addr, addr_len) < 0) {
		fprintf(stderr, "[!] Failed to transmit packet\n");
		close(l3fd);
		return 1;
	}

	ssize_t ret = 0;
	struct sockaddr_in src_addr;
	int len = 65535;
	char buffer[len];
	while (g_data.loop) {
		ret = recvfrom(l3fd, buffer, len, MSG_DONTWAIT,
			(struct sockaddr *)&src_addr, &addr_len);
		if (ret > 0) {
			struct iphdr *newip = (struct iphdr*)buffer;
			dprintf(STDOUT_FILENO, "Received from ");
			print_ip(STDOUT_FILENO, (uint8_t*)&newip->saddr);
			dprintf(STDOUT_FILENO, " to ");
			print_ip(STDOUT_FILENO, (uint8_t*)&newip->daddr);
			dprintf(STDOUT_FILENO, "\n");

			ft_bzero(&dst_addr, sizeof(dst_addr));
			dst_addr.sin_family = AF_INET;

			newip->daddr = inet_addr("172.17.0.3");
			newip->saddr = inet_addr("8.8.8.8");

			ft_memcpy(&dst_addr.sin_addr, &newip->daddr, sizeof(newip->daddr));

			printf("[*] Transmitting packet\n");
			if (sendto(l3fd, newip, ret, 0, (struct sockaddr*)&dst_addr, addr_len) < 0) {
				fprintf(stderr, "[!] Failed to transmit packet\n");
				close(l3fd);
				return 1;
			}

			ft_bzero(buffer, len);
			break;
		}
	}

	close(l3fd);
	return 0;
}

static int sniff_traffic(void *osef)
{
	/* TODO: Verbose */
	(void)osef;
	int l2fd;
	struct ethernet_hdr *ethernet;
	struct iphdr *ip;
	int len = 65535;
	char buffer[len];
	ssize_t ret;
	struct sockaddr_ll src_addr;
	socklen_t addr_len = sizeof(struct sockaddr_ll);

	ft_bzero(buffer, len);

	if (g_data.opt & OPT_VERBOSE)
		printf("[*] Creating AF_PACKET socket for sniffer\n");
	l2fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

	if (l2fd < 0)
		fprintf(stderr, "[!] Failed to open layer 2 socket\n");

	if (g_data.opt & OPT_VERBOSE)
		printf("[*] Starting the sniffing loop\n");
	while (g_data.loop) {
		ret = recvfrom(l2fd, buffer, len, MSG_DONTWAIT,
			(struct sockaddr *)&src_addr, &addr_len);
		if (ret > 0) {
			if (g_data.opt & OPT_BROADCAST ||
				(!filter_out(g_data.source_mac, src_addr.sll_addr, ETH_ADDR_LEN) ||
				!filter_out(g_data.target_mac, src_addr.sll_addr, ETH_ADDR_LEN))) {
				ethernet = (struct ethernet_hdr *)buffer;
				//print_mac(src_addr.sll_addr);
				if (ft_htons(ethernet->type) == ETHERTYPE_IP) {
					ip = (struct iphdr *)(buffer + sizeof(struct ethernet_hdr));
					void *layer4 = buffer + sizeof(struct ethernet_hdr)
						+ sizeof(struct iphdr);
					if (ip->protocol == IPPROTO_ICMP)
						print_icmp(layer4, ip);
					else if (ip->protocol == IPPROTO_TCP)
						print_tcp(layer4, ip,
						ret - (sizeof(struct ethernet_hdr) + sizeof(struct iphdr)
						+ sizeof(struct tcphdr)));
					//else if (ip->protocol == IPPROTO_UDP)
					//	print_udp(layer4, ip);
					(void)print_udp;
				}

				transmit_packet(ip, ret - sizeof(struct ethernet_hdr));

			}
			ft_bzero(buffer, len);
		}
	}

	(void)print_icmp;

	if (g_data.opt & OPT_VERBOSE)
		printf("[*] Closing sniffer's socket\n");

	close(l2fd);

	return 0;
}

int launch_thread(pthread_t *thread)
{
	if (g_data.opt & OPT_VERBOSE)
		printf("[*] Starting sniffer thread\n");
	if (pthread_create(thread, NULL,
		(void*)sniff_traffic, NULL) != 0)
	{
		fprintf(stderr, "[!] Failed to create the sniffer thread\n");
		return -1;
	}

	return 0;
}
