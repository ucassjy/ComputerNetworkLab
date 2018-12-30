#include "tcp.h"
#include "tcp_sock.h"
#include "tcp_timer.h"

#include "log.h"
#include "ring_buffer.h"

#include <stdlib.h>
// update the snd_wnd of tcp_sock
//
// if the snd_wnd before updating is zero, notify tcp_sock_send (wait_send)
static inline void tcp_update_window(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u16 old_snd_wnd = tsk->snd_wnd;
	tsk->snd_wnd = cb->rwnd;
	if (old_snd_wnd == 0)
		wake_up(tsk->wait_send);
}

// update the snd_wnd safely: cb->ack should be between snd_una and snd_nxt
static inline void tcp_update_window_safe(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	if (less_or_equal_32b(tsk->snd_una, cb->ack) && less_or_equal_32b(cb->ack, tsk->snd_nxt))
		tcp_update_window(tsk, cb);
}

#ifndef max
#	define max(x,y) ((x)>(y) ? (x) : (y))
#endif

// check whether the sequence number of the incoming packet is in the receiving
// window
static inline int is_tcp_seq_valid(struct tcp_sock *tsk, struct tcp_cb *cb)
{
	u32 rcv_end = tsk->rcv_nxt + max(tsk->rcv_wnd, 1);
	if (less_than_32b(cb->seq, rcv_end) && less_or_equal_32b(tsk->rcv_nxt, cb->seq_end)) {
		return 1;
	}
	else {
		log(ERROR, "received packet with invalid seq, drop it.");
		return 0;
	}
}

// Process the incoming packet according to TCP state machine. 
void tcp_process(struct tcp_sock *tsk, struct tcp_cb *cb, char *packet)
{
	// fprintf(stdout, "TODO: implement %s please.\n", __FUNCTION__);
	if (cb->seq == tsk->rcv_nxt) {
		tsk->snd_una = cb->ack;
		tsk->rcv_nxt = cb->seq_end;
	}
	struct snd_buf *sndb, *q = NULL;
	switch (cb->flags) {
		case TCP_RST:
			tcp_set_state(tsk, TCP_CLOSED);
			tcp_unhash(tsk);
		case TCP_SYN:
			if (TCP_LISTEN == tsk->state) {
				struct tcp_sock *child_sock = alloc_tcp_sock();
				child_sock->sk_sip = cb->daddr;
				child_sock->sk_sport = tsk->sk_sport;
				child_sock->sk_dip = cb->saddr;
				child_sock->sk_dport = cb->sport;
				child_sock->parent = tsk;
				child_sock->state = tsk->state;
				child_sock->iss = tcp_new_iss();
				child_sock->snd_una = tsk->snd_una;
				child_sock->snd_nxt = tsk->iss;
				child_sock->rcv_nxt = tsk->rcv_nxt;
				struct sock_addr skaddr = {htonl(cb->daddr), htons(cb->dport)};
				tcp_sock_bind(child_sock, &skaddr);
				tcp_set_state(child_sock, TCP_SYN_RECV);
				list_add_tail(&child_sock->list, &tsk->listen_queue);

				tcp_send_control_packet(child_sock, TCP_SYN|TCP_ACK);
				tcp_hash(child_sock);
			}
			break;
		case TCP_SYN | TCP_ACK:
			if (TCP_SYN_SENT == tsk->state) {
				wake_up(tsk->wait_connect);
			}
			break;
		case TCP_ACK:
			list_for_each_entry_safe(sndb, q, &tsk->snd_buffer, list) {
				if (sndb->seq_end <= cb->ack) {
					if (sndb->packet)
						free(sndb->packet);
					list_delete_entry(&sndb->list);
				}
			}
			update_timer(tsk);
			if (TCP_SYN_RECV == tsk->state) {
				list_delete_entry(&tsk->list);
				tcp_sock_accept_enqueue(tsk);
				tcp_set_state(tsk, TCP_ESTABLISHED);
				wake_up(tsk->parent->wait_accept);
			} else if (TCP_FIN_WAIT_1 == tsk->state) {
				tcp_set_state(tsk, TCP_FIN_WAIT_2);
			} else if (TCP_LAST_ACK == tsk->state) {
				tcp_set_state(tsk, TCP_CLOSED);
				if (!tsk->parent)
					tcp_bind_unhash(tsk);
				tcp_unhash(tsk);
			}
			break;
		case TCP_PSH | TCP_ACK:
			if (cb->seq != tsk->rcv_nxt) {
				struct rcv_win rcvw;
				rcvw.packet = (char*)malloc(cb->pl_len * sizeof(char));
				memcpy(rcvw.packet, cb->payload, cb->pl_len);
				rcvw.len = cb->pl_len;
				rcvw.seq_num = cb->seq;
				list_add_tail(&rcvw.list, &tsk->rcv_ofo_buffer);
			} else if (TCP_ESTABLISHED == tsk->state) {
				pthread_mutex_lock(&tsk->rcv_buf->lock);
				write_ring_buffer(tsk->rcv_buf, cb->payload, cb->pl_len);
				pthread_mutex_unlock(&tsk->rcv_buf->lock);
				tsk->rcv_wnd = ring_buffer_free(tsk->rcv_buf);
				struct rcv_win *rcvw, *q;
				list_for_each_entry_safe(rcvw, q, &tsk->rcv_ofo_buffer, list) {
					if (rcvw->seq_num == tsk->rcv_nxt) {
						pthread_mutex_lock(&tsk->rcv_buf->lock);
						write_ring_buffer(tsk->rcv_buf, rcvw->packet, rcvw->len);
						pthread_mutex_unlock(&tsk->rcv_buf->lock);
						tsk->rcv_wnd = ring_buffer_free(tsk->rcv_buf);
						tsk->rcv_nxt += rcvw->len;
					}
					free(rcvw->packet);
					list_delete_entry(&rcvw->list);
				}
				tcp_send_control_packet(tsk, TCP_ACK);
			}
			break;
		case TCP_FIN | TCP_ACK:
			if (TCP_ESTABLISHED == tsk->state) {
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_CLOSE_WAIT);
			} else if (TCP_FIN_WAIT_2 == tsk->state || TCP_FIN_WAIT_1 == tsk->state) {
				tcp_send_control_packet(tsk, TCP_ACK);
				tcp_set_state(tsk, TCP_TIME_WAIT);
			}
	}
}
