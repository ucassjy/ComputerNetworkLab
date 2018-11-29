#include "mospf_daemon.h"
#include "mospf_proto.h"
#include "mospf_nbr.h"
#include "mospf_database.h"

#include "ip.h"
#include "packet.h"

#include "list.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

extern ustack_t *instance;

pthread_mutex_t mospf_lock;

void mospf_init()
{
	pthread_mutex_init(&mospf_lock, NULL);

	instance->area_id = 0;
	// get the ip address of the first interface
	iface_info_t *iface = list_entry(instance->iface_list.next, iface_info_t, list);
	instance->router_id = iface->ip;
	instance->sequence_num = 0;
	instance->lsuint = MOSPF_DEFAULT_LSUINT;

	iface = NULL;
	list_for_each_entry(iface, &instance->iface_list, list) {
		iface->helloint = MOSPF_DEFAULT_HELLOINT;
		init_list_head(&iface->nbr_list);
	}

	init_mospf_db();
}

void *sending_mospf_hello_thread(void *param);
void *sending_mospf_lsu_thread(void *param);
void *checking_nbr_thread(void *param);

void mospf_run()
{
	pthread_t hello, lsu, nbr;
	pthread_create(&hello, NULL, sending_mospf_hello_thread, NULL);
	pthread_create(&lsu, NULL, sending_mospf_lsu_thread, NULL);
	pthread_create(&nbr, NULL, checking_nbr_thread, NULL);
}

void *sending_mospf_hello_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF Hello message periodically.\n");
	int size = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE;
	while (1) {
		pthread_mutex_lock(&mospf_lock);
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list) {
			char *packet = malloc(size * sizeof(char));
			memset(packet, 0, size);
			struct ether_header *ehdr = (struct ether_header*)packet;
			ehdr->ether_dhost[5] = 0x05;
			ehdr->ether_dhost[2] = 0x5e;
			ehdr->ether_dhost[0] = 0x01;
			memcpy(ehdr->ether_shost, iface->mac, ETH_ALEN);
			ehdr->ether_type = htons(ETH_P_IP);
			struct iphdr *ihdr = packet_to_ip_hdr(packet);
			ip_init_hdr(ihdr, iface->ip, MOSPF_ALLSPFRouters, size - ETHER_HDR_SIZE, IPPROTO_MOSPF);
			struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_BASE_HDR_SIZE);
			struct mospf_hello *mhhdr = (struct mospf_hello*)((char*)mhdr + MOSPF_HDR_SIZE);
			mospf_init_hello(mhhdr, iface->mask);
			mospf_init_hdr(mhdr, MOSPF_TYPE_HELLO, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, instance->router_id, instance->area_id);
			mhdr->checksum = mospf_checksum(mhdr);
			iface_send_packet(iface, packet, size);
		}
		pthread_mutex_unlock(&mospf_lock);
		sleep(MOSPF_DEFAULT_HELLOINT);
	}
	return NULL;
}

void *checking_nbr_thread(void *param)
{
	// fprintf(stdout, "TODO: neighbor list timeout operation.\n");
	while (1) {
		iface_info_t *iface = NULL;
		pthread_mutex_lock(&mospf_lock);
		list_for_each_entry(iface, &instance->iface_list, list) {
			mospf_nbr_t *nbr = NULL, *q;
			list_for_each_entry_safe(nbr, q, &iface->nbr_list, list) {
				nbr->alive ++;
				if (nbr->alive > 3 * iface->helloint) {
					list_delete_entry(&nbr->list);
					free(nbr);
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);
		sleep(1);
	}
	return NULL;
}

void handle_mospf_hello(iface_info_t *iface, const char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF Hello message.\n");
	struct iphdr *ihdr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_HDR_SIZE(ihdr));
	pthread_mutex_lock(&mospf_lock);
	int find = 0;
	mospf_nbr_t *nbr = NULL;
	list_for_each_entry(nbr, &iface->nbr_list, list) {
		if (nbr->nbr_id == ntohl(mhdr->rid)) {
			find = 1;
			nbr->alive = 0;
			break;
		}
	}
	if (0 == find) {
		fprintf(stdout, "DEBUG: receive new hello packet.\n");
		struct mospf_hello *mhhdr = (struct mospf_hello*)((char*)mhdr + MOSPF_HDR_SIZE);
		mospf_nbr_t *new = (mospf_nbr_t*)malloc(sizeof(mospf_nbr_t));
		new->nbr_id = ntohl(mhdr->rid);
		new->nbr_ip = ntohl(ihdr->saddr);
		new->nbr_mask = ntohl(mhhdr->mask);
		new->alive = 0;
		list_add_tail(&new->list, &iface->nbr_list);
		iface->num_nbr++;
	}
	pthread_mutex_unlock(&mospf_lock);
}

void *sending_mospf_lsu_thread(void *param)
{
	// fprintf(stdout, "TODO: send mOSPF LSU message periodically.\n");
	int _size = ETHER_HDR_SIZE + IP_BASE_HDR_SIZE + MOSPF_HDR_SIZE + MOSPF_LSU_SIZE;
	while (1) {
		pthread_mutex_lock(&mospf_lock);
		int num = 0;
		iface_info_t *iface = NULL;
		list_for_each_entry(iface, &instance->iface_list, list) {
			if (0 == iface->num_nbr) num++;
			else num += iface->num_nbr;
		}
		struct mospf_lsa *lsarr = (struct mospf_lsa*)malloc(num * MOSPF_LSA_SIZE);
		int i = 0;
		list_for_each_entry(iface, &instance->iface_list, list) {
			if (0 == iface->num_nbr) {
				lsarr[i].subnet = htonl(iface->ip & iface->mask);
				lsarr[i].mask = htonl(iface->mask);
				lsarr[i].rid = htonl(0);
				i++;
			} else {
				mospf_nbr_t *nbr = NULL;
				list_for_each_entry(nbr, &iface->nbr_list, list) {
					lsarr[i].subnet = htonl(nbr->nbr_ip & nbr->nbr_mask);
					lsarr[i].mask = htonl(nbr->nbr_mask);
					lsarr[i].rid = htonl(nbr->nbr_id);
					i++;
				}
			}
		}
		list_for_each_entry(iface, &instance->iface_list, list) {
			if (iface->num_nbr) {
				mospf_nbr_t *nbr = NULL;
				list_for_each_entry(nbr, &iface->nbr_list, list) {
					int size = _size + num * MOSPF_LSA_SIZE;
					char *packet = (char*)malloc(size * sizeof(char));
					memset(packet, 0, size);
					struct ether_header *ehdr = (struct ether_header*)packet;
					memcpy(ehdr->ether_shost, iface->mac, ETH_ALEN);
					ehdr->ether_type = htons(ETH_P_IP);
					struct iphdr *ihdr = packet_to_ip_hdr(packet);
					ip_init_hdr(ihdr, iface->ip, nbr->nbr_ip, size - ETHER_HDR_SIZE, IPPROTO_MOSPF);
					struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_BASE_HDR_SIZE);
					struct mospf_lsu *lsuhdr = (struct mospf_lsu*)((char*)mhdr + MOSPF_HDR_SIZE);
					mospf_init_lsu(lsuhdr, num);
					u32 *narray = (u32*)((char*)lsuhdr + MOSPF_LSU_SIZE);
					memcpy(narray, lsarr, num * MOSPF_LSA_SIZE);
					mospf_init_hdr(mhdr, MOSPF_TYPE_LSU, MOSPF_HDR_SIZE + MOSPF_HELLO_SIZE, instance->router_id, instance->area_id);
					mhdr->checksum = mospf_checksum(mhdr);
					ip_send_packet(packet, size);
				}
			}
		}
		pthread_mutex_unlock(&mospf_lock);
		instance->sequence_num ++;
		sleep(MOSPF_DEFAULT_LSUINT);
	}
	return NULL;
}

void handle_mospf_lsu(iface_info_t *iface, char *packet, int len)
{
	// fprintf(stdout, "TODO: handle mOSPF LSU message.\n");
	struct iphdr *ihdr = packet_to_ip_hdr(packet);
	struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_HDR_SIZE(ihdr));
	struct mospf_lsu *lsuhdr = (struct mospf_lsu*)((char*)mhdr + MOSPF_HDR_SIZE);
	int num = ntohl(lsuhdr->nadv);
	u32 *narray = (u32*)((char*)lsuhdr + MOSPF_LSU_SIZE);
	int find = 0;
	mospf_db_entry_t *db_entry = NULL;
	list_for_each_entry(db_entry, &mospf_db, list) {
		if (db_entry->rid == ntohl(mhdr->rid)) {
			find = 1;
			if (db_entry->seq < ntohs(lsuhdr->seq)) {
				db_entry->rid = ntohl(mhdr->rid);
				db_entry->seq = ntohs(lsuhdr->seq);
				db_entry->nadv = num;
				db_entry->array = (struct mospf_lsa*)realloc(db_entry->array, num * MOSPF_LSA_SIZE);
				for (int i = 0; i < num; i++) {
					db_entry->array[i].subnet = ntohl(narray[i * 3]);
					db_entry->array[i].mask = ntohl(narray[i * 3 + 1]);
					db_entry->array[i].rid = ntohl(narray[i * 3 + 2]);
				}
			}
		}
	}
	if (0 == find) {
		fprintf(stdout, "DEBUG: recieve new lsu packet, with %d lsa.\n", num);
		mospf_db_entry_t *new = (mospf_db_entry_t*)malloc(sizeof(mospf_db_entry_t));
		new->rid = ntohl(mhdr->rid);
		new->seq = ntohl(lsuhdr->seq);
		new->nadv = num;
		new->array = (struct mospf_lsa*)malloc(num * MOSPF_LSA_SIZE);
		for (int i = 0; i < num; i++) {
			new->array[i].subnet = ntohl(narray[i * 3]);
			new->array[i].mask = ntohl(narray[i * 3 + 1]);
			new->array[i].rid = ntohl(narray[i * 3 + 2]);
		}
		list_add_tail(&new->list, &mospf_db);
	}
	if (--lsuhdr->ttl > 0) {
		iface_info_t *iface_t = NULL;
		list_for_each_entry(iface_t, &instance->iface_list, list) {
			if (iface_t->num_nbr && (iface->index != iface_t->index)) {
				char *_packet = (char*)malloc(len);
				memcpy(_packet, packet, len);
				struct ether_header *ehdr = (struct ether_header*)_packet;
				struct iphdr *ihdr = packet_to_ip_hdr(_packet);
				struct mospf_hdr *mhdr = (struct mospf_hdr *)((char *)ihdr + IP_HDR_SIZE(ihdr));
				memcpy(ehdr->ether_shost, iface->mac, ETH_ALEN);
				mospf_nbr_t *nbr = NULL;
				list_for_each_entry(nbr, &iface_t->nbr_list, list) {
					if (nbr->nbr_id == ntohl(mhdr->rid)) continue;
					mhdr->checksum = mospf_checksum(mhdr);
					ihdr->saddr = htonl(iface->ip);
					ihdr->daddr = htonl(nbr->nbr_ip);
					ihdr->checksum = ip_checksum(ihdr);
					ip_send_packet(_packet, len);
				}
			}
		}
	}
	if (0 == find) {
		fprintf(stdout, "MOSPF Database entries:\n");
		list_for_each_entry(db_entry, &mospf_db, list) {
			for (int i = 0; i < db_entry->nadv; i++) {
				fprintf(stdout, IP_FMT"\t"IP_FMT"\t"IP_FMT"\t"IP_FMT"\n", \
						HOST_IP_FMT_STR(db_entry->rid), HOST_IP_FMT_STR(db_entry->array[i].subnet), \
						HOST_IP_FMT_STR(db_entry->array[i].mask), HOST_IP_FMT_STR(db_entry->array[i].rid));
			}
		}
	}
}

void handle_mospf_packet(iface_info_t *iface, char *packet, int len)
{
	struct iphdr *ip = (struct iphdr *)(packet + ETHER_HDR_SIZE);
	struct mospf_hdr *mospf = (struct mospf_hdr *)((char *)ip + IP_HDR_SIZE(ip));

	if (mospf->version != MOSPF_VERSION) {
		log(ERROR, "received mospf packet with incorrect version (%d)", mospf->version);
		return ;
	}
	if (mospf->checksum != mospf_checksum(mospf)) {
		log(ERROR, "received mospf packet with incorrect checksum");
		return ;
	}
	if (ntohl(mospf->aid) != instance->area_id) {
		log(ERROR, "received mospf packet with incorrect area id");
		return ;
	}

	// log(DEBUG, "received mospf packet, type: %d", mospf->type);

	switch (mospf->type) {
		case MOSPF_TYPE_HELLO:
			handle_mospf_hello(iface, packet, len);
			break;
		case MOSPF_TYPE_LSU:
			handle_mospf_lsu(iface, packet, len);
			break;
		default:
			log(ERROR, "received mospf packet with unknown type (%d).", mospf->type);
			break;
	}
}
