#include "arp.h"
#include "base.h"
#include "types.h"
#include "packet.h"
#include "ether.h"
#include "arpcache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "log.h"

// only init the variables with same values
void init_arp_packet(char* packet, iface_info_t *iface) {
	memcpy(packet + 6, iface->mac, ETH_ALEN);
	struct ether_header *tmp = (struct ether_header*)packet;
	tmp->ether_type = htons(ETH_P_ARP);
	struct ether_arp* ahdr = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	ahdr->arp_hrd = htons(0x01);
	ahdr->arp_pro = htons(0x0800);
	ahdr->arp_hln = 6;
	ahdr->arp_pln = 4;
	memcpy(ahdr->arp_sha, packet + 6, ETH_ALEN);
	ahdr->arp_spa = htonl(iface->ip);
	return;
}

// send an arp request: encapsulate an arp request packet, send it out through
// iface_send_packet
void arp_send_request(iface_info_t *iface, u32 dst_ip)
{
	// fprintf(stderr, "TODO: send arp request when lookup failed in arpcache.\n");
	char *packet = (char*)malloc(ETHER_HDR_SIZE + sizeof(struct ether_arp));
	init_arp_packet(packet, iface);
	memset(packet, 0xff, ETH_ALEN);
	struct ether_arp* ahdr = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	ahdr->arp_op = htons(ARPOP_REQUEST);
	memset(ahdr->arp_tha, 0, ETH_ALEN);
	ahdr->arp_tpa = htonl(dst_ip);
	iface_send_packet(iface, packet, ETHER_HDR_SIZE + sizeof(struct ether_arp));
}

// send an arp reply packet: encapsulate an arp reply packet, send it out
// through iface_send_packet
void arp_send_reply(iface_info_t *iface, struct ether_arp *req_hdr)
{
	// fprintf(stderr, "TODO: send arp reply when receiving arp request.\n");
	char *packet = (char*)malloc(ETHER_HDR_SIZE + sizeof(struct ether_arp));
	init_arp_packet(packet, iface);
	memcpy(packet, req_hdr->arp_sha, ETH_ALEN);
	struct ether_arp* ahdr = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	ahdr->arp_op = htons(ARPOP_REPLY);
	memcpy(ahdr->arp_tha, req_hdr->arp_sha, ETH_ALEN);
	ahdr->arp_tpa = req_hdr->arp_spa;
	iface_send_packet(iface, packet, ETHER_HDR_SIZE + sizeof(struct ether_arp));
}

void handle_arp_packet(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stderr, "TODO: process arp packet: arp request & arp reply.\n");
	struct ether_arp* in_ahdr = (struct ether_arp*)(packet + ETHER_HDR_SIZE);
	if (ntohl(in_ahdr->arp_tpa) == iface->ip) {
		if (ntohs(in_ahdr->arp_op) == ARPOP_REQUEST) {
			arp_send_reply(iface, in_ahdr);
		} else {
			arpcache_insert(ntohl(in_ahdr->arp_spa), in_ahdr->arp_sha);
		}
	} else {
		free(packet);
	}
}

// send (IP) packet through arpcache lookup 
//
// Lookup the mac address of dst_ip in arpcache. If it is found, fill the
// ethernet header and emit the packet by iface_send_packet, otherwise, pending 
// this packet into arpcache, and send arp request.
void iface_send_packet_by_arp(iface_info_t *iface, u32 dst_ip, char *packet, int len)
{
	struct ether_header *eh = (struct ether_header *)packet;
	memcpy(eh->ether_shost, iface->mac, ETH_ALEN);
	eh->ether_type = htons(ETH_P_IP);

	u8 dst_mac[ETH_ALEN];
	int found = arpcache_lookup(dst_ip, dst_mac);
	if (found) {
		// log(DEBUG, "found the mac of %x, send this packet", dst_ip);
		memcpy(eh->ether_dhost, dst_mac, ETH_ALEN);
		iface_send_packet(iface, packet, len);
	}
	else {
		// log(DEBUG, "lookup %x failed, pend this packet", dst_ip);
		arpcache_append_packet(iface, dst_ip, packet, len);
	}
}
