#include "ip.h"
#include "icmp.h"
#include "packet.h"
#include "arpcache.h"
#include "rtable.h"
#include "arp.h"

#include "log.h"

#include <stdlib.h>

// initialize ip header 
void ip_init_hdr(struct iphdr *ip, u32 saddr, u32 daddr, u16 len, u8 proto)
{
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0;
	ip->tot_len = htons(len);
	ip->id = rand();
	ip->frag_off = htons(IP_DF);
	ip->ttl = DEFAULT_TTL;
	ip->protocol = proto;
	ip->saddr = htonl(saddr);
	ip->daddr = htonl(daddr);
	ip->checksum = ip_checksum(ip);
}

// lookup in the routing table, to find the entry with the same and longest prefix.
// the input address is in host byte order
rt_entry_t *longest_prefix_match(u32 dst)
{
	rt_entry_t *entry = NULL, *selected = NULL;

	list_for_each_entry(entry, &rtable, list) {
		if ((dst & entry->mask) == (entry->dest & entry->mask)) {
			if (!selected || selected->mask < entry->mask)
				selected = entry;
		}
	}

	return selected;
}

// determine the next hop for the destination IP address
u32 get_next_hop(rt_entry_t *entry, u32 dst)
{
	if (entry->gw)
		return entry->gw;
	else
		return dst;
}

// send IP packet
void ip_send_packet(char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 dst = ntohl(ip->daddr);
	rt_entry_t *entry = longest_prefix_match(dst);
	if (!entry) {
		log(ERROR, "Could not find forwarding rule for IP (dst:"IP_FMT") packet.", 
				HOST_IP_FMT_STR(dst));
		free(packet);
		return ;
	}

	u32 next_hop = get_next_hop(entry, dst);
	struct ether_header *eh = (struct ether_header *)packet;
	eh->ether_type = ntohs(ETH_P_IP);
	memcpy(eh->ether_shost, entry->iface->mac, ETH_ALEN);

	iface_send_packet_by_arp(entry->iface, next_hop, packet, len);
}
