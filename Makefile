MAN1 =	/usr/man/man1
MAN5 =	/usr/man/man5
BIN =	/usr/local/bin

CFLAGS =	-O
CC =	cc $(CFLAGS)

copytape:	copytape.c
	$(CC) -o copytape copytape.c

install:	copytape
	install -s -m 0511 copytape ${BIN}

man:	man1 man5

man1:	${MAN1}/copytape.1
	cp copytape.1 ${MAN1}

man5:	${MAN5}/copytape.5
	cp copytape.5 ${MAN5}
