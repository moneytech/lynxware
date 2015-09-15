/*
 *	mkpwd: generates random passwords.
 *
 *
 *	Still needs some attention...
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static unsigned int randrange(unsigned int s, unsigned int d)
{
	unsigned int c;
	int f = -1;
	struct timeval t; memset(&t, 0, sizeof t);

	if (d < s) return s;

	f = open("/dev/urandom", O_RDONLY);
	if (f == -1) f = open("/dev/random", O_RDONLY);
	if (f == -1) goto _rand;
	read(f, &c, sizeof(unsigned int));
	close(f);
	goto _norand;

_rand:
	gettimeofday(&t, NULL);
	c ^= (((t.tv_sec * t.tv_usec) * t.tv_sec) << 15) ^ t.tv_usec;
	srandom(c);
	c = random();

_norand:
	c = c % (d - s + 1 == 0 ? d - s : d - s + 1) + s;

	return c;
}

static int getrandc(void)
{
	return randrange(0, 0xff);
}


int main(int argc, char **argv)
{
	char *progname = basename(argv[0]);

	int num = 0;
	int spec = 0;
	char c = 0;
	char *filter = NULL;
	int i = 0, j;

	if (argc < 2) {
_usage:
		printf("Usage: %s <number> [mode] [filter]\n"
			"  where <number> is number of chars to output\n"
			"  [mode] is:\n"
			"\n"
			"   0: print chars from a-zA-Z0-9 array (default)\n"
			"   1: print all printable ascii chars\n"
			"   2: same as 0 but with space\n"
			"   3: same as 1 but with space\n"
			"   4: output every char, without filtering\n"
			"   5: supply own array filter (specify in [filter])\n"
			"   6: (ignored)\n"
			"   7: print from base64 range\n"
			"   8: print from [0-9] array (dec)\n"
			"   9: print from [0-f] array (hex)\n"
			"  10: print from [0-F] array (upper hex)\n"
			"\n"
		, progname);
		exit(1);
	}

	num = atoi(argv[1]);
	if (num < 0) goto _usage;

	if (argv[2]) {
		spec = atoi(argv[2]);
		if (spec < 0 || spec > 10) goto _usage;
	}

	if (spec == 5) {
		if (!argv[3] || !strlen(argv[3]))
			goto _usage;

		filter = argv[3];
	}

	while (i < num) {
		c = getrandc();

		switch (spec) {
			case 0:
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
					|| (c >= '0' && c <= '9')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 1:
				if ((c >= '!' && c <= '~')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 2:
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
					|| (c >= '0' && c <= '9') || (c == ' ')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 3:
				if ((c >= ' ' && c <= '~')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 4:
				fputc(c, stdout);
				fflush(stdout);
				i++;
				break;
			case 5:
				for (j = 0; j < strlen(filter); j++)
					if (c == filter[j]) {
						fputc(c, stdout);
						fflush(stdout);
						i++;
					}
				break;
			case 7:
				if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
					|| (c >= '0' && c <= '9') || (c == '+') || (c == '/')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 8:
				if (c >= '0' && c <= '9') {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 9:
				if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			case 10:
				if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')) {
					fputc(c, stdout);
					fflush(stdout);
					i++;
				}
				break;
			default:
				i++;
				break;
		}
	}

	fputc('\n', stdout);
	fflush(stdout);

	return 0;
}
