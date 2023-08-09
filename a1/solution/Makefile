CC=gcc
INCLUDEDIR=/local/courses/csse2310/include
LIBDIR=/local/courses/csse2310/lib
CFLAGS=-std=gnu99 -Wall -pedantic -g -I$(INCLUDEDIR)
# Uncomment the following line if you want to use the getopt version of
# command line parsing
#CFLAGS += -DUSE_GETOPT
LDFLAGS=-L$(LIBDIR) -lcsse2310a1

PROG=uqwordiply

$(PROG): $(PROG).o

clean:
	rm -f *.o $(PROG)
