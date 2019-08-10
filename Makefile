CC = gcc
CFLAGS = -std=c11 -O2 -Wall

OBJ = gpio.o error.o atmega8-programmer.o 

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

atmega8-programmer: $(OBJ)
	$(CC) $(CFLAGS) -o atmega8-programmer $(OBJ)
	
clean: 
	rm -f *.o atmega8-programmer
