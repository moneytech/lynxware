#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static char blk[4096];
static size_t n, t;

static void usage(void)
{
	printf("usage: xor file 67\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int fd;
	int key;
	char *p, *q;

	if (argc < 3) usage();

	key = atoi(*(argv+2));

	fd = open(*(argv+1), O_RDWR);
	if (fd == -1) return 1;

	errno = 0;
	while (1) {
		n = read(fd, blk, sizeof(blk));
		if (!n) break; t += n;
		p = q = blk;
		while (q-p < n) {
			*(q) ^= key;
			q++;
		}
		lseek(fd, t-n, SEEK_SET);
		write(fd, blk, n);
	}

	close(fd);

	return 0;
}
