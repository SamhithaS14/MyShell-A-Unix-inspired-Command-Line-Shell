CC = gcc
CFLAGS = -g -Wall -Wvla -std=c99 -O2 -fsanitize=address,undefined


all: mysh

arraylist.o: arraylist.c arraylist.h
	$(CC) $(CFLAGS) -c arraylist.c -o arraylist.o

mysh.o: mysh.c arraylist.h
	$(CC) $(CFLAGS) -c mysh.c -o mysh.o

mysh: mysh.o arraylist.o
	$(CC) $(CFLAGS) mysh.o arraylist.o -o mysh

clean:
	rm -f mysh.o arraylist.o mysh
