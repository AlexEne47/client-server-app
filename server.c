#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <math.h>
#include "structs.h"

void send_to_server(int sockfd, char *buffer) {

    // Creez packet-ul TCP
    TCP_packet packet;
    packet.size = strlen(buffer);
    strcpy(packet.payload, buffer);

    int bytes_received = packet.size + sizeof(uint32_t);
    int bytes_remaining = packet.size + sizeof(uint32_t);
    int bytes_sent;

    do {
	bytes_sent = send(sockfd, &packet + (bytes_received - bytes_remaining), bytes_remaining, 0);
	DIE(bytes_sent < 0, "send");
    bytes_remaining -= bytes_sent;
    } while(bytes_remaining > 0);
}

int main(int argc, char *argv[])
{
	int sockfd, UDPsockdf, newsockfd, fdmax, is_exit = 0, client_connected = -1;
	char buffer[BUFLEN];
	struct client clients[MAX_CLIENTS_NO] = {0};
	fd_set read_fds, tmp_fds;	

	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Adresa server-ului 
    struct sockaddr_in server_addr;
	memset((char *) &server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

	// Socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");
    UDPsockdf = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(UDPsockdf < 0, "socket");
    
    // Dezactivez algoritmul Nagle
    int flag = 1;
    int rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(rc < 0, "nagle");

	// Bind
	rc = bind(sockfd, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
	DIE(rc < 0, "bind");
    rc = bind(UDPsockdf, (struct sockaddr *) &server_addr, sizeof(struct sockaddr));
    DIE(rc < 0, "bind");

	// Listen
	rc = listen(sockfd, LISTEN_NO);
	DIE(rc < 0, "listen");

	// Adaugam socket-urile in multime
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	FD_SET(sockfd, &read_fds);
    FD_SET(UDPsockdf, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);

    // Selectam cel mai mare socket
    if(UDPsockdf > sockfd)
	fdmax = UDPsockdf;
    else
    fdmax = sockfd;

	while (1) {
    // Comanda de iesire
    if(is_exit == 1)
    break;

    tmp_fds = read_fds; 
    
    rc = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
    DIE(rc < 0, "select");

    for (int i = 0; i <= fdmax; i++) {
    if (FD_ISSET(i, &tmp_fds)) {
    if(i == UDPsockdf) { // Primim date de la UDP   
        UDP_packet date = (UDP_packet)malloc(sizeof(struct UDP_packet));

        struct sockaddr_in cliaddr;
        socklen_t len = sizeof(cliaddr);
        memset(&cliaddr, 0, sizeof(cliaddr));

        uint8_t semn, putere;
        uint32_t aux;
        uint16_t mod;
        float nr;

        memset(buffer, 0, BUFLEN);
        int rc = recvfrom(UDPsockdf, date, sizeof(struct UDP_packet), 0, (struct sockaddr *)&cliaddr, &len);
        DIE(rc < 0, "recvfrom");

        date->client_addr = cliaddr;

        // Creez buffer-ul 
        memset(buffer, 0, BUFLEN);
        strcpy(buffer, date->topic);
        strcat(buffer, " - ");

        // Tratez cazurile pentru tipurile de date
        switch (date->tip_date)
        {
        case 0:
            strcat(buffer, "INT - ");
            char num[20];
            memcpy(&semn, date->continut, 1);
            memcpy(&aux, date->continut+1, 4);

            if(!semn) 
            sprintf(num, "%d", ntohl(aux));
            else 
            sprintf(num, "%d", -ntohl(aux));

            strcat(buffer, num);
            break;
        
        case 1:
            strcat(buffer, "SHORT_REAL - ");
            char temp[10];

            memcpy(&mod, date->continut, 2);
            nr = (float)ntohs(mod)/100;

            sprintf(temp, "%.2f", nr);
            strcat(buffer, temp);
            break;

        case 2:
            strcat(buffer, "FLOAT - ");
            char temps[10];

            memcpy(&semn, date->continut, 1);
            memcpy(&aux, date->continut+1, 4);
            memcpy(&putere, date->continut+5, 1);

            nr = (float)ntohl(aux);
            while(putere) {
            nr/=10;
            putere--;
            }

            if(semn)
            nr = -nr;

            sprintf(temps, "%f", nr);
            strcat(buffer, temps);
            break;

        case 3:
            strcat(buffer, "STRING - ");
            strcat(buffer, date->continut);
            break;
        
        default:
            break;
        }

        buffer[strlen(buffer)] = '\0';

        for(int j=0; j<MAX_CLIENTS_NO; j++)
        if(clients[j].socket == 1 && clients[j].is_subbed == 1) {
            for(int k=0; k<clients[j].nr_topics; k++) 
            if(strcmp(clients[j].topics[k], date->topic) == 0) 
            send_to_server(j, buffer);

        } else if(clients[j].socket == 0 && clients[j].is_subbed == 1) { // SF
            for(int k=0; k<clients[j].nr_topics; k++) 
            if(strcmp(clients[j].topics[k], date->topic) == 0 && clients[j].is_SF[k] == 1) { 
            int nr = clients[j].nr_SF_topics;
            strcpy(clients[j].SF_topics[nr], buffer);
            clients[j].nr_SF_topics++; 
            }
        }

    } else if (i == sockfd) {
        // Primit conexiune
        struct sockaddr_in cliaddr;
        socklen_t len = sizeof(struct sockaddr_in);

        newsockfd = accept(i, (struct sockaddr *) &cliaddr, &len);
        DIE(newsockfd < 0, "accept");
        clients[newsockfd].socket = 1;
        clients[newsockfd].client_addr = cliaddr;

        // Setam ca am primit conexiune 
        client_connected = 1;

        // Adaugam noul socket in multime
        FD_SET(newsockfd, &read_fds);
        if (newsockfd > fdmax) { 
            fdmax = newsockfd;
        }

    } else if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
        // Input tastatura
        memset(buffer, 0, BUFLEN);
        fgets(buffer, BUFLEN - 1, stdin);
        buffer[strlen(buffer)-1] = '\0';


        if (strncmp(buffer, "exit", 4) == 0) {
            // Anuntam clientii ca s-a inchis server-ul
            for(int j=0; j<MAX_CLIENTS_NO; j++)
            if(clients[j].socket == 1)
            send_to_server(j, buffer);
            is_exit = 1;
            break;
        }

    } else {

        // Primit date
        memset(buffer, 0, BUFLEN);
        int ret = recv(i, buffer, sizeof(buffer), 0);
        DIE(ret < 0, "recv");

        if (ret == 0) {
            // Deconectam clientul
            struct client empty = {0};
            
            for(int j=0; j<empty.nr_topics; j++)
            strcpy(empty.topics[j], clients[i].topics[j]);

            printf("Client %s disconnected.\n", clients[i].ID);
            clients[i].socket = 0;
            close(i);
            FD_CLR(i, &read_fds);

        } else {

            if(client_connected == 1) {
                int cli_found = 0;
                strcpy(clients[i].ID, buffer);

                for(int j=0; j<MAX_CLIENTS_NO && j != i; j++)
                if(strcmp(clients[i].ID, clients[j].ID) == 0)
                cli_found = 1;

                if(cli_found) { // Inchidem conexiunea
                printf("Client %s already connected.\n", clients[i].ID);

                send_to_server(i, "exit");

                strcpy(clients[i].ID, "");
                clients[i].socket = 0;
                close(i);
                FD_CLR(i, &read_fds);

                } else {
                // S-a conectat client nou
                printf("New client %s connected from %s:%d\n",
                clients[i].ID, inet_ntoa(clients[i].client_addr.sin_addr), 
                ntohs(clients[i].client_addr.sin_port));

                // Verificam SF
                for(int j=0; j<clients[i].nr_SF_topics; j++) {
                send_to_server(i, clients[i].SF_topics[j]);
                strcpy(clients[i].SF_topics[j], "");
                }

                clients[i].nr_SF_topics = 0;
                }

                client_connected = -1;
            } else {
                // Am primit comanda de la client
                char command[50];
                char topic[51];
                int SF;

                char *p = strtok(buffer, " ");
                strcpy(command, p);

                if(strcmp(command,"subscribe") == 0) {
                    p = strtok(NULL, " ");
                    strcpy(topic, p);

                    p = strtok(NULL, " ");
                    SF = atoi(p);

                    if(topic[strlen(topic) - 1] == '\n')
                    topic[strlen(topic) - 1] = '\0';

                    clients[i].is_subbed = 1;
                    int nr = clients[i].nr_topics;
                    if(SF == 1)
                    clients[i].is_SF[nr] = 1;
                    else
                    clients[i].is_SF[nr] = 0;
                    strcpy(clients[i].topics[nr], topic);
                    clients[i].nr_topics++;

                    // Trimitem mesaj catre client
                    memset(buffer, 0, BUFLEN);
                    strcpy(buffer, "Subscribed to topic.");
                    send_to_server(i, buffer);

                } else if(strcmp(command,"unsubscribe") == 0) {
                    p = strtok(NULL, " ");
                    strcpy(topic, p);

                    clients[i].is_subbed = 0;
                    for(int j=0; j<clients[i].nr_topics; j++)
                    if(strcmp(clients[i].topics[j], topic) == 0) 
                    strcpy(clients[i].topics[j], "");
                    
                    // Trimitem mesaj catre client
                    memset(buffer, 0, BUFLEN);
                    strcpy(buffer, "Unsubscribed from topic.");
                    buffer[strlen(buffer)] = '\0';
                    send_to_server(i, buffer);
                }
            }
        }
    }}}
    }

    // Inchidem socket-ul
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	close(sockfd);

	return 0;
}
