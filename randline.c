#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#define _s(x) (sizeof(x)/sizeof(*x))

static char fallbackline[8192];

static void xerror(const char *s)
{
	perror(s);
	exit(2);
}

static void usage(void)
{
	printf("usage: randline [-z] [-n NRLINES] [file] ...\n");
	printf("  reads entire file or stream into memory, then prints\n");
	printf("  a random line from it. Multiple streams unified into one.\n\n");
	printf("  -z: suppress empty lines\n");
	printf("  -n NRLINES: print this number of random lines (default - 1)\n\n");
	exit(1);
}

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
	FILE *f[256], *m;
	char *p = NULL, *line = NULL;
	size_t n, x, t, z, largestline = 0;
	unsigned int linecount, randomline;
	int printlines = 1, noempty = 0, c;

	opterr = 0;
	while ((c = getopt(argc, argv, "zn:")) != -1) {
		switch (c) {
			case 'n':
				printlines = atoi(optarg);
				break;
			case 'z':
				noempty = 1;
				break;
			default: usage(); break;
		}
	}

	memset(f, 0, _s(f) * sizeof(*f));

	if (!*(argv+optind)) *(f) = stdin;
	else {
		for (x = 0; x < _s(f) && *(argv+x+optind); x++) {
			if (!strcmp(*(argv+x+optind), "-")) *(f+x) = stdin;
			else {
				*(f+x) = fopen(*(argv+x+optind), "r");
				if (!*(f+x)) perror(*(argv+x+optind));
			}
		}
	}

	for (n = 0, x = 0; x < _s(f); x++) if (*(f+x)) n++;
	if (!n) return 2;

	linecount = z = n = 0;
	for (x = 0; x < _s(f); x++) {
		if (*(f+x)) {
			while (1) {
				t = getline(&p, &n, *(f+x));
				if (t == -1) break;
				if (t > largestline) largestline = t+1;
				line = realloc(line, z+t);
				if (!line) xerror("realloc");
				memset(line+z, 0, t);
				memcpy(line+z, p, t); z += t;
				linecount++;
			}
		}
	}
	if (!linecount) return 0;

	for (x = 0; x < _s(f); x++) {
		if (*(f+x)) fclose(*(f+x));
	}
	free(p);

	p = line;
	m = fmemopen(p, z, "r");
	if (!m) xerror("fmemopen");

	line = malloc(largestline);
	if (!line) {
		line = fallbackline;
		largestline = sizeof(fallbackline);
	}
	memset(line, 0, largestline);

	while (printlines--) {
		randomline = randrange(0, linecount - 1); t = 0;
		fseek(m, 0L, SEEK_SET);
		while (fgets(line, largestline, m)) {
			if (randomline == t) {
				if (noempty && *line == '\n') continue;
				fputs(line, stdout);
				fflush(stdout);
				break;
			}
			t++;
		}
	}

	fclose(m);
	free(p);

	return 0;
}
