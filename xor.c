#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static char kblk[1024], dblk[1024];
static size_t kn, n;

static void usage(const char *fname)
{
	if (fname) fprintf(stderr, "error opening %s: %m\n", fname);
	printf("usage: xor padfile infile outfile\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int keyfd, infd, outfd;
	char *p, *q;

	if (argc < 4) usage(NULL);

	keyfd = open(*(argv+1), O_RDONLY);
	if (keyfd == -1) usage(*(argv+1));

	infd = open(*(argv+2), O_RDONLY);
	if (infd == -1) usage(*(argv+2));

	outfd = open(*(argv+3), O_CREAT|O_WRONLY, 0666);
	if (outfd == -1) usage(*(argv+3));

	errno = 0;
	while (1) {
		n = read(infd, dblk, sizeof(dblk));
		if (!n) break;

		kn = read(keyfd, kblk, sizeof(kblk));
		if (kn < n) break;

		p = q = dblk;
		while (q-p < n) {
			*(q) ^= *(kblk+(q-p));
			q++;
		}

		write(outfd, dblk, n);
	}

	memset(kblk, 0, sizeof(kblk));
	memset(dblk, 0, sizeof(dblk));

	close(keyfd);
	close(infd);
	close(outfd);

	return 0;
}