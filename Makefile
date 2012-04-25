CFLAGS = -Wall

critters: critters.o forf.o

clean:
	rm -f *.o critters
