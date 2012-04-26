CFLAGS = -Wall -Werror

critters: critters.o forf.o

clean:
	rm -f *.o critters
