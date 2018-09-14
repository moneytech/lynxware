#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "getpasswd.c"

#define CSTR(s) (sizeof(s)-1)
#define NOSIZE ((size_t)-1)

static int getps_filter(struct getpasswd_state *getps, char chr, size_t pos)
{
	if (chr == '\x03') { /* ^C */
		getps->retn = ((size_t)-2);
		return 6;
	}
	return 1;
}

static char passwd[256];
static char *prompt;
static struct getpasswd_state getps;

int main(int argc, char **argv)
{
	size_t x;

	prompt = getenv("GETPWD_PROMPT");
	if (!prompt) prompt = "Password:";

	memset(&getps, 0, sizeof(struct getpasswd_state));
	getps.fd = getps.efd = -1;
	getps.passwd = passwd;
	getps.pwlen = sizeof(passwd);
	getps.echo = prompt;
	getps.charfilter = getps_filter;
	getps.maskchar = 'x';
	x = xgetpasswd(&getps);
	if (x == NOSIZE) return 2;
	if (x == ((size_t)-2)) return 1;

	write(1, passwd, strlen(passwd));
	write(1, "\n", CSTR("\n"));

	memset(passwd, 0, sizeof(passwd));

	return 0;
}
