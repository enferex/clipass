APP=clipass
CC=gcc
CFLAGS=-O0 -g3 -Wall -pedantic -std=c11 -D_XOPEN_SOURCE
LDFLAGS=-lX11

$(APP): main.c
	$(CC) $^ $(CFLAGS) $(LDFLAGS) -o $@

clean:
	$(RM) $(APP)
