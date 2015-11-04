SRCS = $(wildcard *.c)
PROGS = $(SRCS:.c=)
override CFLAGS += -Wall -Os

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

gmsgbox: gmsgbox.c
	$(CC) $(CFLAGS) `pkg-config --cflags gtk+-2.0` -o $@ $< `pkg-config --libs gtk+-2.0`

huntbins: huntbins.c
	$(CC) $(CFLAGS) -o $@ $< -lmagic

clean:
	rm -f $(PROGS)
