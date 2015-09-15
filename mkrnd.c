/*
 * mkrnd - generate random number
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
	fprintf(stdout, "\nusage: mkrnd (u64)start (u64)end\n");
	fprintf(stdout, "generates random uint64_t number between start and end\n\n");
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

int main(int argc, char **argv)
{
	if (argc < 3) usage();

	printf("%lld\n", randrangel(atoll(*(argv+1)), atoll(*(argv+2))));

	return 0;
}
