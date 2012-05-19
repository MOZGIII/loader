CC=gcc
CFLAGS=-c -Wall
LDFLAGS=
SOURCES=main.c recvline.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=server.exe

all: clean $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLE)
