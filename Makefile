CFLAGS = -Wall -g

all: server subscriber 

server: server.c

subscriber: subscriber.c

clean:
	rm -f server subscriber 
