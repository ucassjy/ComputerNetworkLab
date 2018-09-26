/* worker application */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
 
int main(int argc, const char *argv[])
{
    int s, cs;
    struct sockaddr_in master, worker;
    char msg[60];
     
    // create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("create socket failed\n");
		return -1;
    }
    printf("socket created\n");
     
    // prepare the sockaddr_in structure
    worker.sin_family = AF_INET;
    worker.sin_addr.s_addr = INADDR_ANY;
    worker.sin_port = htons(12345);
     
    // bind
    if (bind(s,(struct sockaddr *)&worker, sizeof(worker)) < 0) {
        perror("bind failed\n");
        return -1;
    }
    printf("bind done\n");
     
    // listen
    listen(s, 3);
    printf("waiting for incoming connections...\n");
     
    // accept connection from an incoming master
    int c = sizeof(struct sockaddr_in);
    if ((cs = accept(s, (struct sockaddr *)&master, (socklen_t *)&c)) < 0) {
        perror("accept failed\n");
        return -1;
    }
    printf("connection accepted\n");
     
	int msg_len = 0;
    // receive an order from master
    while ((msg_len = recv(cs, msg, sizeof(msg), 0)) > 0) {
        // work and send the result back to master
		int len = ntohl(((int*)msg)[0]);
		char sted[8];
		memcpy(sted, msg + 4, 8);
		int start = ntohl(((int*)sted)[0]);
		int length = ntohl(((int*)sted)[1]) - start + 1;
		printf("start = %d, length = %d\n", start, length);
		char *fn = malloc(len * sizeof(char));
		memcpy(fn, msg + 12, len);
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
		fclose(fp);
		void *cres = malloc(104);
		int tmp;
		for (int i = 0; i < 26; i++) {
			printf("%c, %d\n", 'a' + i, ires[i]);
			tmp = htonl(ires[i]);
			memcpy(cres + i * 4, &tmp, 4);
		}
        write(cs, cres, 104);
    }
     
    if (msg_len == 0) {
        printf("client disconnected\n");
    } else { // msg_len < 0
        perror("recv failed\n");
		return -1;
    }
     
    return 0;
}
