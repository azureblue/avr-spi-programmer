CFLAGS = -std=c11 -O2 -Wall
#CFLAGS = -std=c11 -g -Wall

CC = gcc

OBJ = gpio.o error.o avr-spi-programmer.o config.o delay.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

avr-spi-programmer: $(OBJ)
	$(CC) $(CFLAGS) -o avr-spi-programmer $(OBJ)

clean:
	rm -f *.o avr-spi-programmer
