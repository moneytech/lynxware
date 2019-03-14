SRCS = $(filter-out xstrlcat.c xstrlcpy.c getpasswd.c, $(wildcard *.c))
PROGS = $(SRCS:.c=)

ifneq (,$(DEBUG))
override CFLAGS+=-Wall -O0 -g
else
override CFLAGS+=-O2
endif

all: $(PROGS)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

huntbins: huntbins.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lmagic

mkhash: mkhash.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -lcrypt

clean:
	rm -f $(PROGS)
