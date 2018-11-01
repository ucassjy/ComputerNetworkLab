#include "ip.h"
#include "icmp.h"
#include "rtable.h"
#include "arp.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>

// forward the IP packet from the interface specified by longest_prefix_match, 
// when forwarding the packet, you should check the TTL, update the checksum,
// determine the next hop to forward the packet, then send the packet by 
// iface_send_packet_by_arp
void ip_forward_packet(u32 ip_dst, char *packet, int len)
{
	// fprintf(stderr, "TODO: forward ip packet.\n");
 	rt_entry_t *rt = longest_prefix_match(ip_dst);
	if (rt) {
		struct iphdr *hdr = packet_to_ip_hdr(packet);
		u8 ttl = hdr->ttl;
		if (ttl <= 1) {
			icmp_send_packet(packet, len, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL);
			return;
		}
		hdr->ttl = --ttl;
		hdr->checksum = ip_checksum(hdr);
		if (rt->gw) iface_send_packet_by_arp(rt->iface, rt->gw, packet, len);
		else  iface_send_packet_by_arp(rt->iface, ip_dst, packet, len);
	} else {
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_NET_UNREACH);
	}
}

// handle ip packet
//
// If the packet is ICMP echo request and the destination IP address is equal to
// the IP address of the iface, send ICMP echo reply; otherwise, forward the
// packet.
void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	if (daddr == iface->ip) {
		// fprintf(stderr, "TODO: reply to the sender if it is ping packet.\n");
		u8 type = (u8)*(packet + ETHER_HDR_SIZE + IP_HDR_SIZE(ip));
		if (type == ICMP_ECHOREQUEST) {
			icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		} else {
			free(packet);
		}
	}
	else {
		ip_forward_packet(daddr, packet, len);
	}
}
