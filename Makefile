all: tinyhttpd
tinyhttpd: tinyhttpd.c
	gcc -g -W -Wall -lpthread -o tinyhttpd tinyhttpd.c

clean:
	rm tinyhttpd
