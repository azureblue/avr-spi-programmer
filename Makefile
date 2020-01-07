CC = gcc
CFLAGS = -std=c11 -g -Wall

OBJ = gpio.o error.o atmega-programmer.o config.o delay.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

atmega8-programmer: $(OBJ)
	$(CC) $(CFLAGS) -o atmega-programmer $(OBJ)
	
clean: 
	rm -f *.o atmega-programmer
