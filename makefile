CC = gcc 
CFLAGS = -Wall -Wextra 
LDLIBS = -luv
main: main.c
	$(CC) $(CFLAGS) main.c -o main $(LDLIBS)
