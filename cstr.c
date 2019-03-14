#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#define NOSIZE ((size_t)-1)

static char xblk[4096], *pblk;
static size_t numcols = 16;
static int givelength;

static void usage(void)
{
	printf("usage: cstr [-l] [-c cols] [-/FILE] [...]\n");
	printf("Format files as a C source strings.\n");
	printf("\n");
	printf("  -: format standard input instead of file.\n");
	printf("  -l: print length of each file as a variable.\n");
	printf("  -c cols: break string by this number of <cols> octets.\n");
	printf("    (0 to disable line breaks to print a lengthy string)\n");
	printf("\n");
	exit(1);
}

static size_t adjust_cols(size_t nc, size_t sz)
{
	return nc ? ((sz / nc) * nc) : sz;
}

int main(int argc, char **argv)
{
	int fd, x, do_stop, c;
	size_t ldone, lrem, lio, t;
	unsigned long long ltotal;
	char *fname;

	opterr = 0;
	while ((c = getopt(argc, argv, "lc:")) != -1) {
		switch (c) {
			case 'c': numcols = atoi(optarg); break;
			case 'l': givelength = 1; break;
			default: usage(); break;
		}
	}

	if (!argv[optind]) {
		fd = 0;
		goto _stdin;
	}

	for (x = optind; argv[x] && x < argc; x++) {
		if (!strcmp(argv[x], "-")) fd = 0;
		else fd = open(argv[x], O_RDONLY);
		if (fd == -1) return 1;

_stdin:		fname = (fd != 0) ? basename(argv[x]) : "stdin";
		ltotal = 0;
		printf("char *%s = \n\"", fname);
		fflush(stdout);

		do_stop = 0;
		while (1) {
			if (do_stop) break;
			pblk = xblk;
			lrem = adjust_cols(numcols, sizeof(xblk));
			ldone = 0;
_ragain:		lio = read(fd, pblk, lrem);
			if (lio == 0) do_stop = 1;
			if (lio != NOSIZE) ldone += lio;
			else return 2;
			if (lio && lio < lrem) {
				pblk += lio;
				lrem -= lio;
				goto _ragain;
			}
			for (t = 0; t < ldone; t++) {
				printf("\\x%02hhx", xblk[t]);
				if (numcols
				&& t
				&& ((t+1) % numcols) == 0) {
					if (!((t+1) == ldone && do_stop)) printf("\"\n\"");
				}
			}
			fflush(stdout);
			ltotal += ldone;
		}

		printf("\";\n");
		close(fd);
		if (givelength) printf("size_t sz_%s = %llu;\n", fname, ltotal);
	}

	return 0;
}
