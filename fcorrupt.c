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
	if (setting == 0) {
		memzero(buf, sz);
	}
	else {
		if (!genrandom(buf, sz)) return -1;
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
	off_t rndoff;
	int x;

	/* fcorrupt file 0/R num max */
	if (argc < 5) xerror("usage: fcorrupt FILE 0|R NM LEN");

	f = fopen(*(argv+1), "r+");
	if (!f) xerror(*(argv+1));

	type = *(*(argv+2)) == '0' ? 0 : 1;
	num = atoi(*(argv+3));
	max = atol(*(argv+4));

	bad = malloc(max);
	if (!bad) xerror("malloc");

	if (fstat(fileno(f), &st) == -1)
		xerror(*(argv+1));

	for (x = 0; x < num; x++) {
		rndsz = (size_t)randrange(1, (unsigned)max);
		rndoff = (off_t)randrangel(0, (long long)st.st_size);
		rndoff -= rndsz;
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
