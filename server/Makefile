CC      = gcc											# Cange for yours
CFLAGS  = -O2 -Wall -Wextra -pedantic -pthread
LDFLAGS = -pthread

all: server

server: server.c
	$(CC) $(CFLAGS) server.c -o server $(LDFLAGS)

clean:
	rm -f server