APP=clipass
CFLAGS=-O0 -g3

$(APP): main.c
	$(CC) $^ $(CFLAGS) -o $@

clean:
	$(RM) $(APP)
