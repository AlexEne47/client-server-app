#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "structs.h"

int main(int argc, char *argv[])
{
	int sockfd;
	char buffer[BUFLEN], ID[10];
	fd_set read_fds, tmp_fds;

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc != 4) {
        printf("\n Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

	// Socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// Dezactivez algoritmul Nagle
    int flag = 1;
    int rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(rc < 0, "nagle");

	// Adresa server-ului
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[3]));
	rc = inet_aton(argv[2], &server_addr.sin_addr);
	DIE(rc == 0, "inet_aton");

	// Connect
	rc = connect(sockfd, (struct sockaddr*) &server_addr, sizeof(server_addr));
	DIE(rc < 0, "connect");

	// Trimit ID-ul catre server
    strcpy(ID, argv[1]);
	rc = send(sockfd, ID, strlen(ID), 0);
	DIE(rc < 0, "send");

	// Adaugam socket-ul in multime
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	while (1) {
	tmp_fds = read_fds; 
	
	rc = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
	DIE(rc < 0, "select");

	if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
		// Se citeste de la tastatura
		memset(buffer, 0, BUFLEN);
		fgets(buffer, BUFLEN - 1, stdin);

		if (strncmp(buffer, "exit", 4) == 0) {
			close(sockfd);
			break;
		}

		// Trimit comanda la server
		rc = send(sockfd, buffer, strlen(buffer), 0);
		DIE(rc < 0, "send");
	}

	if (FD_ISSET(sockfd, &tmp_fds)) {
		// Primim date de la server
		uint32_t length;

		rc = recv(sockfd, &length, sizeof(uint32_t), 0);
		DIE(rc < 0 || rc != 4, "recv");

		int buflen = length;
		int bytes_received = length;
		int bytes_remaining = length;
		int bytes_sent;

		do {
		bytes_sent = recv(sockfd, &buffer + (bytes_received - bytes_remaining), bytes_remaining, 0);
		DIE(bytes_sent < 0, "recv");
		bytes_remaining -= bytes_sent;
		} while(bytes_remaining > 0);

		buffer[buflen] = '\0';

		if(rc == 0) {
			printf("Server closed the connection.\n");
			break;
		}
		else if(strcmp(buffer, "exit") == 0) {
			break;
		} else
		printf("%s\n", buffer);
	}}

	// Inchidem socket-ul
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	close(sockfd);

	return 0;
}
