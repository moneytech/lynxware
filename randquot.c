/*
 * Random quotes emitter.
 *
 * Syntax: randquot quotes.txt
 *
 * Layout of input file:
 *
 * # comment
 * {
 * quote text
 * 	quote author (example)
 * };
 * {
 *    ** another quote text **
 *
 *    who said that??
 * };
 * ...
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
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

int main(int argc, char **argv)
{
	FILE *fp;
	char *p = NULL; size_t n = 0, nn = 0;
	int state = 0, count = 1;
	unsigned randqu = 0, nrquot = 0;
	char *rp = NULL;

	if (argc < 2) return 1;

	fp = fopen(*(argv+1), "r");
	if (!fp) return 2;

getrnd:	while (getdelim(&p, &n, '\n', fp) != -1) {
		n = strlen(p);
		if (!state && (!*(p) || *(p) == '\n' || *(p) == '#')) goto again;
		if (*(p) == '{') {
			state = 1;
			nrquot++;
			goto again;
		}
		else if (!strncmp(p, "};", 2)) {
			state = 0;
		}
		if (randqu == nrquot && state && !count) { /* get it */
			n = strlen(p); nn += n;
			rp = realloc(rp, nn+1);
			if (!rp) {
				free(p);
				break;
			}
			memset(rp+nn-n, 0, n+1);
			strlcpy(rp+nn-n, p, n+1);
		}
again:		free(p); p = NULL;
	}

	if (!nrquot) goto end;

	if (count) {
		fseek(fp, 0L, SEEK_SET);
		randqu = randrange(1, nrquot);
		count = nrquot = 0;
		goto getrnd;
	}

end:	fclose(fp);

	if (!state && rp) {
		n = strlen(rp); *(rp+n-1) = '\0';
		puts(rp);
	}
	else if (state) {
		fputs("unclosed or malformed quote section\n", stderr);
		return 3;
	}

	return 0;
}
