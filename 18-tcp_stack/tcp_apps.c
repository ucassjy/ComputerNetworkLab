#include "tcp_sock.h"

#include "log.h"

#include <unistd.h>

// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_worker(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));

	struct tcp_sock *csk = tcp_sock_accept(tsk);

	log(DEBUG, "accept a connection.");

	char rbuf[101];
	void *wbuf = malloc(201);
	int rlen = 0;
	while (1) {
		rlen = tcp_sock_read(csk, rbuf, 100);
		if (rlen == 0)
			continue;
		printf("Receive packet with length %d.\n", rlen);
		// work and send the result back to master
		int len = ntohl(((int*)rbuf)[0]);
		char sted[8];
		memcpy(sted, rbuf + 4, 8);
		int start = ntohl(((int*)sted)[0]);
		int length = ntohl(((int*)sted)[1]) - start + 1;
		printf("start = %d, length = %d\n", start, length);
		char *fn = malloc(len * sizeof(char));
		memcpy(fn, rbuf + 12, len);
		printf("fn = %s\n", fn);
		FILE *fp = fopen(fn, "r");
		fseek(fp, start, SEEK_SET);
		int ires[26] = {0};
		for (int i = 0; i < length; i++) {
			char c = fgetc(fp);
			if (c >= 'a' && c <= 'z')
				ires[c - 'a']++;
			if (c >= 'A' && c <= 'Z')
				ires[c - 'A']++;
		}
		int tmp;
		for (int i = 0; i < 26; i++) {
			printf("%c, %d\n", 'a' + i, ires[i]);
			tmp = htonl(ires[i]);
			memcpy(wbuf + i * 4, &tmp, 4);
		}
		if (tcp_sock_write(csk, (char*)wbuf, 200) < 0) {
			log(DEBUG, "tcp_sock_write return negative value, finish transmission.");
			break;
		}
		break;
	}

	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);

	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_master(void *arg)
{
	u16 port = *(u16 *)arg;
	struct sock_addr *skaddr[3];
	for (int i = 0; i < 3; i++) {
		skaddr[i] = (struct sock_addr*)malloc(sizeof(struct sock_addr));;
		skaddr[i]->port = port;
	}
	skaddr[0]->ip = inet_addr("10.0.0.2");
	skaddr[1]->ip = inet_addr("10.0.0.3");
	skaddr[2]->ip = inet_addr("10.0.0.4");

	// get the length of the file
	char *fname = "war_and_peace.txt";
	int fnlen = strlen(fname);
	FILE *fp;
	fp = fopen(fname, "r");
	int size = 0;
	while (fgetc(fp) != EOF)
		size++;
	fclose(fp);
	int split = size / 3;

	// create socket
	struct tcp_sock *tsk[3];
	for (int i = 0; i < 3; i++) {
		tsk[i] = alloc_tcp_sock();
	}

	// connect socket
	for (int i = 0; i < 3; i++) {
		if (tcp_sock_connect(tsk[i], skaddr[i]) < 0) {
			log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
					NET_IP_FMT_STR(skaddr[i]->ip), ntohs(skaddr[i]->port));
			exit(1);
		}
	}

	char wbuf[3][101];
	char rbuf[3][501];
	int rlen = 0;
	for (int i = 0; i < 3; i++) {
		int tmp;
		tmp = htonl(fnlen);
		memcpy(wbuf[i], &tmp, 4);
		tmp = htonl(split * i);
		memcpy(wbuf[i] + 4, &tmp, 4);
		if (i == 2) {
			tmp = htonl(size);
		} else {
			tmp = htonl(split * (i + 1) - 1);
		}
		memcpy(wbuf[i] + 8, &tmp, 4);
		memcpy(wbuf[i] + 12, fname, fnlen);

		// send the orders to the workers
		if (tcp_sock_write(tsk[i], wbuf[i], 100) < 0) {
			printf("send %d failed\n", i);
			break;
		}
	}

	// receive replies from the workers
	for (int i = 0; i < 3; i++) {
		while (1) {
			rlen = tcp_sock_read(tsk[i], rbuf[i], 500);
			if (rlen < 0) {
				log(DEBUG, "tcp_sock_read return negative value, finish transmission.");
				break;
			}
			else if (rlen > 0) {
				fprintf(stdout, "length of read data[%d] = %d.\n", i, rlen);
				break;
			}
		}
	}

	for (int i = 0; i < 26; i++) {
		int res = 0;
		res += ntohl(((int*)rbuf[0])[i]);
		res += ntohl(((int*)rbuf[1])[i]);
		res += ntohl(((int*)rbuf[2])[i]);
		printf("%c %d\n", 'a' + i, res);
	}

	for (int i = 0; i < 3; i++) {
		tcp_sock_close(tsk[i]);
	}

	return NULL;
}
