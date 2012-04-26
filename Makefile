CFLAGS = -Wall -Werror

all: critters critters.cgi forf.html

critters: critters.o forf.o cgi.o

critters.cgi: critters
	ln -s $< $@

%: %.m4
	m4 $< > $@

clean:
	rm -f *.o critters critters.cgi
