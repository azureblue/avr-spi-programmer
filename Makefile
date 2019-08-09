CC=gcc

atmega8-programmer: atmega8-programmer.c
	$(CC) atmega8-programmer.c -O2 -Wall -std=c11 -o atmega8-programmer


