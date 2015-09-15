/*
 * rngd(8) little replacement program
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/random.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>

static char *source = "/dev/hwrandom", *dest = "/dev/random";
static int oub = 256, fill = 2048, delay = 400000;

typedef struct {
	int cnt;
	int bufsiz;
	uint8_t *buf;
} randpool_t;

static void usage(void)
{
	printf("usage: rngdtiny [-orsWT]\n\n");
	printf("  -o dev: output random device\n");
	printf("  -r dev: input random device\n");
	printf("  -s num: byte block at a iteration\n");
	printf("  -W num: stop filling if more than num bytes available in pool\n");
	printf("  -T delay: delay between reads. Measured in microseconds\n\n");
	exit(1);
}

static void xerror(const char *s)
{
	perror(s);
	exit(2);
}

int main(int argc, char **argv)
{
	int a, b, avail;
	size_t x;
	randpool_t p;
	int c;

	opterr = 0;
	optind = 1;
	while ((c = getopt(argc, argv, "o:r:s:W:T:")) != -1) {
		switch (c) {
			case 'o':
				dest = optarg;
				break;
			case 'r':
				source = optarg;
				break;
			case 's':
				oub = atoi(optarg);
				break;
			case 'W':
				fill = atoi(optarg);
				break;
			case 'T':
				delay = atoi(optarg);
				break;
			default: usage(); break;
		}
	}

	a = ((*source == '-' && !*(source+1)) ? 0 : open(source, O_RDONLY));
	if (a == -1) xerror(source);
	b = ((*dest == '-' && !*(dest+1)) ? 0 : open(dest, O_RDWR));
	if (b == -1) xerror(dest);

	p.buf = malloc(oub);
	if (!p.buf) xerror("malloc");
	memset(p.buf, 0, oub);

	while (1) {
		if (ioctl(b, RNDGETENTCNT, &avail) != 0) { perror("ioctl"); goto _again; }
		if (avail > fill) goto _again;

		x = read(a, p.buf, oub);
		p.cnt = x*CHAR_BIT;
		p.bufsiz = x;

		ioctl(b, RNDADDENTROPY, &p);

_again:
		usleep(delay);
	}

/* The program usually interrupted by signal, so it's cleaned up by the OS */

	return 0;
}
