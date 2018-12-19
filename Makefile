CFLAGS=-std=c99 -O3 -Wall
OBJS=eoip.o sock.o tap.o eoip-proto.o

eoip: $(OBJS)
	$(CC) $(CFLAGS) -o eoip $(OBJS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: install
install:
	cp eoip /usr/local/sbin/eoip

.PHONY: uninstall
uninstall:
	rm -f /usr/local/sbin/eoip

.PHONY: clean
clean:
	rm -f eoip
	rm -f *.o
