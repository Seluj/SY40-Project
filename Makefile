CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: peage

peage: main.o
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f thread_example *.o