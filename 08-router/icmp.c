#include "icmp.h"
#include "ip.h"
#include "rtable.h"
#include "arp.h"
#include "base.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// send icmp packet
void icmp_send_packet(const char *in_pkt, int len, u8 type, u8 code)
{
	// fprintf(stderr, "TODO: malloc and send icmp packet.\n");
	struct ether_header *in_ehdr = (struct ether_header*)in_pkt;
	struct iphdr *in_ihdr = packet_to_ip_hdr(in_pkt);
	int pkt_len;
	if (type == ICMP_ECHOREPLY) pkt_len = len;
	else pkt_len = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + IP_HDR_SIZE(in_ihdr) + ICMP_HDR_SIZE + 8;

	char *packet = (char*)malloc(pkt_len * sizeof(char));
	struct ether_header *ehdr = (struct ether_header*)packet;
	memcpy(ehdr->ether_dhost, in_ehdr->ether_shost, ETH_ALEN);
	memcpy(ehdr->ether_shost, in_ehdr->ether_dhost, ETH_ALEN);
	ehdr->ether_type = htons(ETH_P_IP);

	struct iphdr *ihdr = packet_to_ip_hdr(packet);
	rt_entry_t *rt = longest_prefix_match(ntohl(in_ihdr->saddr));
	ip_init_hdr(ihdr, rt->iface->ip, ntohl(in_ihdr->saddr), pkt_len - ETHER_HDR_SIZE, 1);

	struct icmphdr *ic = (struct icmphdr*)(packet + ETHER_HDR_SIZE + IP_BASE_HDR_SIZE);
	ic->type = type;
	ic->code = code;
	int hdr_len = ETHER_HDR_SIZE + IP_HDR_SIZE(ihdr) + 4;
	if (type == ICMP_ECHOREPLY) {
		memcpy(packet + hdr_len, in_pkt + hdr_len, pkt_len - hdr_len);
	} else {
		memset(packet + hdr_len, 0, 4);
		memcpy(packet + hdr_len + 4, in_pkt + ETHER_HDR_SIZE, IP_HDR_SIZE(in_ihdr) + 8);
	}
	ic->checksum = icmp_checksum(ic, pkt_len - ETHER_HDR_SIZE - IP_BASE_HDR_SIZE);
	ip_send_packet(packet, pkt_len);
}
