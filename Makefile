SRCS = $(wildcard *.c)
PROGS = $(SRCS:.c=)
override CFLAGS += -Wall -Os

GTK2_CFLAGS:=`pkg-config --cflags gtk+-2.0`
GTK2_LDFLAGS:=`pkg-config --libs gtk+-2.0`

XFORMS_CFLAGS:=-I/local/include/freetype2
XFORMS_LDFLAGS:=-lforms -lfreetype -L/local/X11/lib -Wl,-rpath-link -Wl,/local/X11/lib -lX11

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

gmsgbox: gmsgbox.c
	$(CC) $(CFLAGS) $(GTK2_CFLAGS) -o $@ $< $(GTK2_LDFLAGS) $(LDFLAGS)

xmkhash: xmkhash.c
	$(CC) $(CFLAGS) $(XFORMS_CFLAGS) -o $@ $< $(XFORMS_LDFLAGS) $(LDFLAGS) -lcrypt

huntbins: huntbins.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lmagic

zdiv: zdiv.c
	$(CC) $(CFLAGS) -O2 -o $@ $< $(LDFLAGS)

unclaws: unclaws.c
	@echo See unclaws.README.

fcorrupt: fcorrupt.c
	@echo fcorrupt uses SANELINUX extensions.

clean:
	rm -f $(PROGS)
