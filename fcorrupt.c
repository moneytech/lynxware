/*
 * fcorrupt program -- corrupt files intentionally.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

static void xerror(const char *s)
{
	if (errno) fprintf(stderr, "%s: %s\n", s, strerror(errno));
	else fprintf(stderr, "%s\n", s);
	exit(2);
}

static ssize_t genbad(void *buf, size_t sz, int setting)
{
	if (setting >= 0) {
		memset(buf, setting, sz);
	}
	else if (setting == -1) {
		setting = randrange(0, UCHAR_MAX);
		memset(buf, setting, sz);
	}
	else if (setting == -2) {
		size_t x;
		unsigned char t[3], *ubuf = (unsigned char *)buf;

		if (genrandom(t, sizeof(t)) == -1) return -1;
		for (x = 0; x < sz; x += sizeof(t))
			memcpy(ubuf+x, t, sizeof(t));
		if (sz-x < sizeof(t)) memcpy(ubuf+x, t, sz-x);
	}
	else {
		if (genrandom(buf, sz) == -1) return -1;
	}
	return sz;
}

int main(int argc, char **argv)
{
	char *bad = NULL;
	int type, num;
	size_t rndsz, max;
	FILE *f;
	struct stat st;
	off_t soff, rndoff;
	int x;

	if (argc < 6) xerror("usage: fcorrupt FILE off 0-255|r|R NM LEN");

	f = fopen(*(argv+1), "r+");
	if (!f) xerror(*(argv+1));

	soff = atoll(*(argv+2));

	if (*(*(argv+3)) == 'r') type = -1;
	else if (*(*(argv+3)) == 'C') type = -2;
	else if (*(*(argv+3)) == 'R') type = -3;
	else type = atoi(*(argv+3));
	if (type > UCHAR_MAX) type = 255;
	num = atoi(*(argv+4));
	max = atol(*(argv+5));

	bad = malloc(max);
	if (!bad) xerror("malloc");

	if (fstat(fileno(f), &st) == -1)
		xerror(*(argv+1));

	for (x = 0; x < num; x++) {
again:		rndsz = (size_t)randrange(1, (unsigned)max);
		rndoff = (off_t)randrangel((long long)soff, (long long)st.st_size);
		rndoff -= rndsz;
		if (rndoff < soff) goto again;
		if (fseek(f, rndoff, SEEK_SET) == -1)
			xerror("fseek");
		if (genbad(bad, rndsz, type) == -1)
			xerror("genbad");
		fprintf(stderr, "%u: writing at off %lld %zu %s bytes...\n",
			x + 1, rndoff, rndsz, type ? "random" : "zero");
		if (fwrite(bad, rndsz, 1, f) != 1)
			xerror("fwrite");
	}

	fclose(f);
	free(bad);

	fprintf(stderr, "%s corruption done!\n", *(argv+1));

	return 0;
}
