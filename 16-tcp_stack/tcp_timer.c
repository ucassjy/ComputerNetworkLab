#include "tcp.h"
#include "tcp_timer.h"
#include "tcp_sock.h"

#include <unistd.h>

static struct list_head timer_list;

// scan the timer_list, find the tcp sock which stays for at 2*MSL, release it
void tcp_scan_timer_list()
{
	struct tcp_sock *tsk;
	struct tcp_timer *t, *q;
	list_for_each_entry_safe(t, q, &timer_list, list) {
		t->timeout -= TCP_TIMER_SCAN_INTERVAL;
		if (t->timeout <= 0) {
			if (TIME_WAIT == t->type) {
				list_delete_entry(&t->list);

				tsk = timewait_to_tcp_sock(t);
				if (! tsk->parent)
					tcp_bind_unhash(tsk);
				tcp_set_state(tsk, TCP_CLOSED);
				free_tcp_sock(tsk);
			} else {
				tsk = retrans_to_tcp_sock(t);
				if (!list_empty(&tsk->snd_buffer)) {
					if (3 == tsk->retrans_num) {
						tcp_sock_close(tsk);
						break;
					} else {
						struct snd_buf *sndb = list_entry(tsk->snd_buffer.next, struct snd_buf, list);
						int len = sizeof(sndb->packet);
						tcp_sock_write(tsk, sndb->packet, len);
						update_timer(tsk);
						tsk->retrans_num++;
					}
				}
			}
		}
	}
}

// set the timewait timer of a tcp sock, by adding the timer into timer_list
void tcp_set_timewait_timer(struct tcp_sock *tsk)
{
	struct tcp_timer *timer = &tsk->timewait;

	timer->type = 0;
	timer->timeout = TCP_TIMEWAIT_TIMEOUT;
	list_add_tail(&timer->list, &timer_list);

	tcp_sock_inc_ref_cnt(tsk);
}

// scan the timer_list periodically by calling tcp_scan_timer_list
void *tcp_timer_thread(void *arg)
{
	init_list_head(&timer_list);
	while (1) {
		usleep(TCP_TIMER_SCAN_INTERVAL);
		tcp_scan_timer_list();
	}

	return NULL;
}

void update_timer(struct tcp_sock *tsk) {
	tsk->retrans_timer.timeout = TCP_RETRANS_INTERVAL;
	for (int i = 0; i < tsk->retrans_num; i++)
		tsk->retrans_timer.timeout *= 2;
}
