CFLAGS = -Wall -Werror

all: critters critters.cgi

critters: critters.o forf.o cgi.o

critters.cgi: critters
	ln -s $< $@

clean:
	rm -f *.o critters critters.cgi
