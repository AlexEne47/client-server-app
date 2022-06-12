#include <stdio.h>
#include <stdlib.h>

#define MAX_PAYLOAD 1550
#define MAX_CLIENTS_NO 100
#define MAX_TOPICS_NO 50
#define BUFLEN 256
#define LISTEN_NO 5
#define FD_START 4
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

typedef struct __attribute__((packed)) UDP_packet {
    char topic[50];
    uint8_t tip_date;
    char continut[1500];
    struct sockaddr_in client_addr;
} *UDP_packet;

typedef struct __attribute__((packed)) TCP_packet {
    uint32_t size;
    char payload[MAX_PAYLOAD];
} TCP_packet;

typedef struct client {
    char ID[10];
    int socket;
    int nr_topics, nr_SF_topics, is_subbed;
    struct sockaddr_in client_addr;
    char topics[MAX_TOPICS_NO][51];
    char SF_topics[MAX_TOPICS_NO][150];
    int is_SF[MAX_TOPICS_NO];
}c;

