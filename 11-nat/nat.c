#include "nat.h"
#include "ip.h"
#include "icmp.h"
#include "tcp.h"
#include "rtable.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static struct nat_table nat;

void handle_ip_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 daddr = ntohl(ip->daddr);
	if (daddr == iface->ip && ip->protocol == IPPROTO_ICMP) {
		struct icmphdr *icmp = (struct icmphdr *)IP_DATA(ip);
		if (icmp->type == ICMP_ECHOREQUEST) {
			icmp_send_packet(packet, len, ICMP_ECHOREPLY, 0);
		}

		free(packet);
	}
	else {
		nat_translate_packet(iface, packet, len);
	}
}

// get the interface from iface name
static iface_info_t *if_name_to_iface(const char *if_name)
{
	iface_info_t *iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		if (strcmp(iface->name, if_name) == 0)
			return iface;
	}

	log(ERROR, "Could not find the desired interface according to if_name '%s'", if_name);
	return NULL;
}

// determine the direction of the packet, DIR_IN / DIR_OUT / DIR_INVALID
static int get_packet_direction(char *packet)
{
	// fprintf(stdout, "TODO: determine the direction of this packet.\n");
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 saddr = ntohl(ip->saddr);
	rt_entry_t *rt = longest_prefix_match(saddr);
	iface_info_t *iface = rt->iface;
	if (iface->index == nat.internal_iface->index) {
		return DIR_OUT;
	} else if (iface->index == nat.external_iface->index) {
		return DIR_IN;
	}
	return DIR_INVALID;
}

u16 assign_external_port() {
	u16 i;
	for (i = NAT_PORT_MIN; i < NAT_PORT_MAX; i++) {
		if (!nat.assigned_ports[i]) {
			nat.assigned_ports[i] = 1;
			break;
		}
	}
	return i;
}

// do translation for the packet: replace the ip/port, recalculate ip & tcp
// checksum, update the statistics of the tcp connection
void do_translation(iface_info_t *iface, char *packet, int len, int dir)
{
	// fprintf(stdout, "TODO: do translation for this packet.\n");
	pthread_mutex_lock(&nat.lock);
	struct iphdr *ip = packet_to_ip_hdr(packet);
	u32 addr = (dir == DIR_IN)? ntohl(ip->saddr) : ntohl(ip->daddr);
	u8 hash_i = hash8((char*)&addr, 4);
	struct list_head *head = &(nat.nat_mapping_list[hash_i]);
	struct nat_mapping *mapping_entry = NULL;
	struct tcphdr *tcp = packet_to_tcp_hdr(packet);
	if (dir == DIR_IN) {
		list_for_each_entry(mapping_entry, head, list) {
			if (mapping_entry->external_ip == ntohl(ip->daddr) && \
				mapping_entry->external_port == ntohs(tcp->dport)) break;
		}
		tcp->dport = htons(mapping_entry->internal_port);
		ip->daddr = htonl(mapping_entry->internal_ip);
		mapping_entry->conn.external_fin = (tcp->flags == TCP_FIN);
		mapping_entry->conn.external_seq_end = tcp->seq;
		if (tcp->flags == TCP_ACK) mapping_entry->conn.external_ack = tcp->ack;
	} else {
		int found = 0;
		list_for_each_entry(mapping_entry, head, list) {
			if (mapping_entry->internal_ip == ntohl(ip->saddr) && \
				mapping_entry->internal_port == ntohs(tcp->sport)) {
				found = 1;
				break;
			}
		}
		if (!found) {
			struct nat_mapping *new_entry = (struct nat_mapping*)malloc(sizeof(struct nat_mapping));
			memset(new_entry, 0, sizeof(struct nat_mapping));
			new_entry->internal_ip = ntohl(ip->saddr);
			new_entry->external_ip = nat.external_iface->ip;
			new_entry->internal_port = ntohs(tcp->sport);
			new_entry->external_port = assign_external_port();
			list_insert(&(new_entry->list), &(mapping_entry->list), mapping_entry->list.next);
			mapping_entry = new_entry;
		}
		tcp->sport = htons(mapping_entry->external_port);
		ip->saddr = htonl(mapping_entry->external_ip);
		mapping_entry->conn.internal_fin = (tcp->flags == TCP_FIN);
		mapping_entry->conn.internal_seq_end = tcp->seq;
		if (tcp->flags == TCP_ACK) mapping_entry->conn.internal_ack = tcp->ack;
	}
	tcp->checksum = tcp_checksum(ip, tcp);
	ip->checksum = ip_checksum(ip);
	mapping_entry->update_time = time(NULL);
	pthread_mutex_unlock(&nat.lock);
	ip_send_packet(packet, len);
}

void nat_translate_packet(iface_info_t *iface, char *packet, int len)
{
	int dir = get_packet_direction(packet);
	if (dir == DIR_INVALID) {
		log(ERROR, "invalid packet direction, drop it.");
		icmp_send_packet(packet, len, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH);
		free(packet);
		return ;
	}

	struct iphdr *ip = packet_to_ip_hdr(packet);
	if (ip->protocol != IPPROTO_TCP) {
		log(ERROR, "received non-TCP packet (0x%0hhx), drop it", ip->protocol);
		free(packet);
		return ;
	}

	do_translation(iface, packet, len, dir);
}

// nat timeout thread: find the finished flows, remove them and free port
// resource
void *nat_timeout()
{
	while (1) {
		// fprintf(stdout, "TODO: sweep finished flows periodically.\n");
		pthread_mutex_lock(&nat.lock);
		time_t now = time(NULL);
		for (int i = 0; i < HASH_8BITS; i++) {
			struct list_head *head = &(nat.nat_mapping_list[i]);
			if (!list_empty(head)) {
				struct nat_mapping *mapping, *q;
				list_for_each_entry_safe(mapping, q, head, list) {
					if (now - mapping->update_time > TCP_ESTABLISHED_TIMEOUT) {
						nat.assigned_ports[mapping->external_port] = 0;
						list_delete_entry(&mapping->list);
						free(mapping);
						continue;
					}
					if (mapping->conn.internal_fin && mapping->conn.external_fin && \
						mapping->conn.internal_ack == mapping->conn.external_seq_end + 1 && \
						mapping->conn.external_ack == mapping->conn.internal_seq_end + 1) {
						nat.assigned_ports[mapping->external_port] = 0;
						list_delete_entry(&mapping->list);
						free(mapping);
					}
				}
			}
		}
		pthread_mutex_unlock(&nat.lock);
		sleep(1);
	}

	return NULL;
}

// initialize nat table
void nat_table_init()
{
	memset(&nat, 0, sizeof(nat));

	for (int i = 0; i < HASH_8BITS; i++)
		init_list_head(&nat.nat_mapping_list[i]);

	nat.internal_iface = if_name_to_iface("n1-eth0");
	nat.external_iface = if_name_to_iface("n1-eth1");
	if (!nat.internal_iface || !nat.external_iface) {
		log(ERROR, "Could not find the desired interfaces for nat.");
		exit(1);
	}

	memset(nat.assigned_ports, 0, sizeof(nat.assigned_ports));

	pthread_mutex_init(&nat.lock, NULL);

	pthread_create(&nat.thread, NULL, nat_timeout, NULL);
}

// destroy nat table
void nat_table_destroy()
{
	pthread_mutex_lock(&nat.lock);

	for (int i = 0; i < HASH_8BITS; i++) {
		struct list_head *head = &nat.nat_mapping_list[i];
		struct nat_mapping *mapping_entry, *q;
		list_for_each_entry_safe(mapping_entry, q, head, list) {
			list_delete_entry(&mapping_entry->list);
			free(mapping_entry);
		}
	}

	pthread_kill(nat.thread, SIGTERM);

	pthread_mutex_unlock(&nat.lock);
}
