/*
 *	New mkhash.c, rewritten.
 *	This code is placed into public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>
#include <termios.h>
#include <fcntl.h>

#include "getpasswd.c"

#define CSTR(s) (sizeof(s)-1)
#define NOSIZE ((size_t)-1)

static char passwd[256];
static char salt[32], *psalt;
static char *hash;
static struct getpasswd_state getps;

static int getps_filter(struct getpasswd_state *getps, char chr, size_t pos)
{
	if (chr == '\x03') { /* ^C */
		getps->retn = ((size_t)-2);
		return 6;
	}
	return 1;
}

static inline int isctrlchr(int c)
{
	if (c == 9) return 0;
	if (c >= 0 && c <= 31) return 1;
	if (c == 127) return 1;
	return 0;
}

static int getps_plain_filter(struct getpasswd_state *getps, char chr, size_t pos)
{
	int x;

	x = getps_filter(getps, chr, pos);
	if (x != 1) return x;

	if (pos < getps->pwlen && !isctrlchr(chr))
		write(getps->efd, &chr, sizeof(char));
	return 1;
}

int main(int argc, char **argv)
{
	size_t x;

	if (argc < 2) {
		memset(&getps, 0, sizeof(struct getpasswd_state));

		getps.fd = getps.efd = -1;
		getps.passwd = passwd;
		getps.pwlen = sizeof(passwd);
		getps.echo = "Password:";
		getps.charfilter = getps_filter;
		getps.maskchar = 'x';
		x = xgetpasswd(&getps);

		getps.fd = getps.efd = -1;
		getps.passwd = salt;
		getps.pwlen = sizeof(salt);
		getps.echo = "Salt:";
		getps.charfilter = getps_plain_filter;
		getps.maskchar = 0;
		x = xgetpasswd(&getps);
		if (x == NOSIZE) return 2;
		if (x == ((size_t)-2)) return 1;
		psalt = salt;
	}
	else {
		if (read(0, passwd, CSTR(passwd)) == -1) return 2;
		psalt = *(argv+1);
	}

	hash = crypt(passwd, psalt);
	memset(passwd, 0, sizeof(passwd));
	if (!hash) return 1;

	write(1, hash, strlen(hash));
	write(1, "\n", CSTR("\n"));

	return 0;
}
