/* master application */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
 
int main(int argc, char *argv[])
{
	if(argc < 2){
		printf("File name needed!");
		exit(0);
	}
    int s[3];
    struct sockaddr_in worker[3];
    void *message[3], *worker_reply[3];
	for(int i = 0; i < 3; i++){
		message[i] = malloc(32);
		worker_reply[i] = malloc(104);
	}
     
    // create socket
	for (int i = 0; i < 3; i++) {
		s[i] = socket(AF_INET, SOCK_STREAM, 0);
		if (s[i] == -1) {
			printf("create socket%d failed\n", i+1);
			return -1;
		}
		printf("Socket%d created\n", i+1);
	}
     
	FILE *fp;
	char ip[3][10];
	if (!(fp = fopen("./workers.conf", "r"))) {
		printf("Open Failed.\n");
		return -1;
	}
	for (int i = 0; i < 3; i++) {
		fgets(ip[i], 10, fp);
		worker[i].sin_addr.s_addr = inet_addr(ip[i]);
		worker[i].sin_family = AF_INET;
		worker[i].sin_port = htons(12345);
	}
	fclose(fp);
 
    // connect to server
	for (int i = 0; i < 3; i++) {
		if (connect(s[i], (struct sockaddr *)&(worker[i]), sizeof(worker[i])) < 0) {
			perror("connect to worker failed\n");
			return 1;
		}
		printf("worker[%d] connected\n", i);
	}
     
	char *fn = argv[1];
	int fnlen = strlen(fn);
	// get the length of the file
	if (!(fp = fopen(fn, "r"))) {
		printf("Open Failed.\n");
		return -1;
	}
	int size = 0;
	while (fgetc(fp) != EOF)
		size++;
	fclose(fp);
	int split = size / 3;

	for (int i = 0; i < 3; i++) {
		int tmp;
		tmp = htonl(fnlen);
		memcpy(message[i], &tmp, 4);
		tmp = htonl(split * i);
		memcpy(message[i] + 4, &tmp, 4);
		if (i == 2) {
			tmp = htonl(size);
		} else {
			tmp = htonl(split * (i + 1) - 1);
		}
		memcpy(message[i] + 8, &tmp, 4);
		memcpy(message[i] + 12, fn, fnlen);

		// send the orders to the workers
		if (send(s[i], message[i], 31, 0) < 0) {
			printf("send %d failed\n", i);
			return 1;
		}
	}

	for (int i = 0; i < 3; i++) {
		// receive replies from the workers
		if (recv(s[i], worker_reply[i], 104, 0) < 0) {
			printf("recv %d failed\n", i);
			continue;
		}
    }

	for (int i = 0; i < 26; i++) {
		int res = 0;
		res += ntohl(((int*)worker_reply[0])[i]);
		res += ntohl(((int*)worker_reply[1])[i]);
		res += ntohl(((int*)worker_reply[2])[i]);
		printf("%c %d\n", 'a' + i, res);
	}

	for (int i = 0; i < 3; i++) {
		close(s[i]);
	}
    return 0;
}
