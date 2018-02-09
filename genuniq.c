/*
 * genuniq - unique random number set generator
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void usage(void)
{
	fprintf(stdout, "\nusage: genuniq (int)num (u64)start (u64)end\n");
	fprintf(stdout, "generates set containing random unique numbers between start and end\n\n");
	fprintf(stdout, "  start - start number\n");
	fprintf(stdout, "  end - end number\n\n");
	exit(1);
}

static uint32_t randrange(uint32_t s, uint32_t d)
{
	uint32_t c;
	int f = -1;
	struct timeval t; memset(&t, 0, sizeof t);

	if (d < s) return s;

	f = open("/dev/urandom", O_RDONLY);
	if (f == -1) f = open("/dev/random", O_RDONLY);
	if (f == -1) goto _rand;
	read(f, &c, sizeof(uint32_t));
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

static uint64_t randrangel(uint64_t s, uint64_t d)
{
	uint64_t c;

	c = randrange(0, (uint32_t)0xffffffff);
	c <<= 32;
	c |= randrange(0, (uint32_t)0xffffffff);

	c = c % (d - s + 1 == 0 ? d - s : d - s + 1) + s;

	return c;
}

static int is_unique(uint64_t *rnd, int idx, uint64_t x)
{
	int y;

	for (y = 0; y < idx; y++) if (rnd[y] == x) return 0;
	return 1;
}

int main(int argc, char **argv)
{
	int x, y;
	uint64_t *rnd, t, s, e;

	if (argc < 4) usage();

	x = y = atoi(*(argv+1));
	if (x == 0) return 0;

	s = atoll(*(argv+2));
	e = atoll(*(argv+3));
	if (s > e) return 1;
	if (e - s < x - 1) return 1;

	rnd = malloc(x * sizeof(uint64_t));
	if (!rnd) return 2;

	for (x = 0; x < y; x++) {
_again:		t = randrangel(s, e);
		if (!is_unique(rnd, x, t)) goto _again;
		rnd[x] = t;
	}

	for (x = 0; x < y; x++) printf("%lld\n", rnd[x]);

	free(rnd);
	return 0;
}
