CC=gcc
CFLAGS=-c -Wall -I../../contrib/wslay/lib/includes
LDFLAGS=-L../../contrib/wslay/lib/.libs -I../../contrib/wslay/lib/includes ../../contrib/wslay/lib/.libs/libwslay.a -lssl
SOURCES=main.c http_handshake.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=server.exe

all: clean $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)
