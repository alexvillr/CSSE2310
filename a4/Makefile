CC=gcc
INCLUDEDIR=/local/courses/csse2310/include
LIBDIR=/local/courses/csse2310/lib
CFLAGS=-std=gnu99 -Wall -pedantic -g -I$(INCLUDEDIR) -pthread -lcrypt
LDFLAGS=-L$(LIBDIR) -lcsse2310a4 -lcsse2310a3 -pthread -lcrypt

CLIENT=crackclient
SERVER=crackserver

all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT).o
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT).o $(LDFLAGS)

$(SERVER): $(SERVER).o
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER).o $(LDFLAGS)

clean:
	rm -f *.o $(CLIENT) $(SERVER)
