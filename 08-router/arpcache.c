#include "arpcache.h"
#include "arp.h"
#include "ether.h"
#include "packet.h"
#include "icmp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static arpcache_t arpcache;

// initialize IP->mac mapping, request list, lock and sweeping thread
void arpcache_init()
{
	bzero(&arpcache, sizeof(arpcache_t));

	init_list_head(&(arpcache.req_list));

	pthread_mutex_init(&arpcache.lock, NULL);

	pthread_create(&arpcache.thread, NULL, arpcache_sweep, NULL);
}

// release all the resources when exiting
void arpcache_destroy()
{
	pthread_mutex_lock(&arpcache.lock);

	struct arp_req *req_entry = NULL, *req_q;
	list_for_each_entry_safe(req_entry, req_q, &(arpcache.req_list), list) {
		struct cached_pkt *pkt_entry = NULL, *pkt_q;
		list_for_each_entry_safe(pkt_entry, pkt_q, &(req_entry->cached_packets), list) {
			list_delete_entry(&(pkt_entry->list));
			free(pkt_entry->packet);
			free(pkt_entry);
		}

		list_delete_entry(&(req_entry->list));
		free(req_entry);
	}

	pthread_kill(arpcache.thread, SIGTERM);

	pthread_mutex_unlock(&arpcache.lock);
}

// lookup the IP->mac mapping
//
// traverse the table to find whether there is an entry with the same IP
// and mac address with the given arguments
int arpcache_lookup(u32 ip4, u8 mac[ETH_ALEN])
{
	// fprintf(stderr, "TODO: lookup ip address in arp cache.\n");
	pthread_mutex_lock(&arpcache.lock);
	for (int i = 0; i < MAX_ARP_SIZE; i++) {
		if (arpcache.entries[i].valid && arpcache.entries[i].ip4 == ip4) {
			memcpy(mac, arpcache.entries[i].mac, ETH_ALEN);
			pthread_mutex_unlock(&arpcache.lock);
			return 1;
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
	return 0;
}

// append the packet to arpcache
//
// Lookup in the list which stores pending packets, if there is already an
// entry with the same IP address and iface (which means the corresponding arp
// request has been sent out), just append this packet at the tail of that entry
// (the entry may contain more than one packet); otherwise, malloc a new entry
// with the given IP address and iface, append the packet, and send arp request.
void arpcache_append_packet(iface_info_t *iface, u32 ip4, char *packet, int len)
{
	// fprintf(stderr, "TODO: append the ip address if lookup failed, and send arp request if necessary.\n");
	int found = 0;
	struct arp_req *wlist = NULL;
	struct cached_pkt *cpkt = (struct cached_pkt*)malloc(sizeof(struct cached_pkt));
	cpkt->packet = packet;
	cpkt->len = len;
	pthread_mutex_lock(&arpcache.lock);
	list_for_each_entry(wlist, &(arpcache.req_list), list) {
		if (wlist->ip4 == ip4 && wlist->iface == iface) {
			found = 1;
			break;
		}
	}
	if (found) {
		list_add_tail(&(cpkt->list), &(wlist->cached_packets));
	} else {
		struct arp_req *req = (struct arp_req*)malloc(sizeof(struct arp_req));
		req->iface = iface;
		req->ip4 = ip4;
		req->sent = time(NULL);
		req->retries = 0;
		init_list_head(&(req->cached_packets));
		list_add_tail(&(cpkt->list), &(req->cached_packets));
		list_add_tail(&(req->list), &(arpcache.req_list));
		arp_send_request(iface, ip4);
	}
	pthread_mutex_unlock(&arpcache.lock);
}

// insert the IP->mac mapping into arpcache, if there are pending packets
// waiting for this mapping, fill the ethernet header for each of them, and send
// them out
void arpcache_insert(u32 ip4, u8 mac[ETH_ALEN])
{
	// fprintf(stderr, "TODO: insert ip->mac entry, and send all the pending packets.\n");
	int found = 0;
	struct arp_req *wlist = NULL, *wlist_q;
	pthread_mutex_lock(&arpcache.lock);
	time_t now = time(NULL);
	for (int i = 0; i < MAX_ARP_SIZE; i++) {
		if (!arpcache.entries[i].valid) {
			found = 1;
			arpcache.entries[i].ip4 = ip4;
			memcpy(arpcache.entries[i].mac, mac, ETH_ALEN);
			arpcache.entries[i].added = now;
			arpcache.entries[i].valid = 1;
			break;
		}
	}
	if (!found) {
		int id = (u16)now % 32;
		arpcache.entries[id].ip4 = ip4;
		memcpy(arpcache.entries[id].mac, mac, ETH_ALEN);
		arpcache.entries[id].added = now;
	}
	list_for_each_entry_safe(wlist, wlist_q, &(arpcache.req_list), list) {
		if (wlist->ip4 == ip4) {
			struct cached_pkt *tmp = NULL, *tmp_q;
			list_for_each_entry_safe(tmp, tmp_q, &(wlist->cached_packets), list) {
				memcpy(tmp->packet, mac, ETH_ALEN);
				iface_send_packet(wlist->iface, tmp->packet, tmp->len);
				free(tmp);
			}
			list_delete_entry(&(wlist->list));
			free(wlist);
		}
	}
	pthread_mutex_unlock(&arpcache.lock);
}

// sweep arpcache periodically
//
// For the IP->mac entry, if the entry has been in the table for more than 15
// seconds, remove it from the table.
// For the pending packets, if the arp request is sent out 1 second ago, while 
// the reply has not been received, retransmit the arp request. If the arp
// request has been sent 5 times without receiving arp reply, for each
// pending packet, send icmp packet (DEST_HOST_UNREACHABLE), and drop these
// packets.
void *arpcache_sweep(void *arg) 
{
	while (1) {
		sleep(1);
		// fprintf(stderr, "TODO: sweep arpcache periodically: remove old entries, resend arp requests .\n");
		struct arp_req *wlist = NULL, *wlist_q;
		pthread_mutex_lock(&arpcache.lock);
		time_t now = time(NULL);
		for (int i = 0; i < MAX_ARP_SIZE; i++) {
			if (arpcache.entries[i].valid && now - arpcache.entries[i].added > ARP_ENTRY_TIMEOUT)
				arpcache.entries[i].valid = 0;
		}
		list_for_each_entry_safe(wlist, wlist_q, &(arpcache.req_list), list) {
			if (wlist->retries > ARP_REQUEST_MAX_RETRIES) {
				struct cached_pkt *tmp = NULL, *tmp_q;
				list_for_each_entry_safe(tmp, tmp_q, &(wlist->cached_packets), list) {
					pthread_mutex_unlock(&arpcache.lock);
					icmp_send_packet(tmp->packet, tmp->len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
					pthread_mutex_lock(&arpcache.lock);
					free(tmp);
				}
				list_delete_entry(&(wlist->list));
				free(wlist);
				continue;
			}
			if (now - wlist->sent > 1) {
				arp_send_request(wlist->iface, wlist->ip4);
				wlist->sent = now;
				wlist->retries += 1;
			}
		}
		pthread_mutex_unlock(&arpcache.lock);
	}

	return NULL;
}
