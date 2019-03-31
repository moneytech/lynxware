#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#include "xstrlcat.c"

#define CONFTOOL_BASE "/etc/conf"

#define OPT_READ 1
#define OPT_WRITE 2
#define OPT_REMOVE 3

#define ifopt { if (opt) usage(); }

static int quiet;
static char data[PATH_MAX];

static void usage(void)
{
	printf("usage: conftool [-qrwd] optionname value\n");
	printf("this tool should be used only with privilege separation program\n");
	exit(1);
}

static void xerror(const char *s)
{
	if (errno && !quiet) perror(s);
	exit(2);
}

static int check_secure_path(const char *p)
{
	if (strstr(p, "..")) return 0;
	if (strchr(p, '/')) return 0;
	return 1;
}

int main(int argc, char **argv)
{
	int c, opt = 0;
	char *optf;
	int fd, x;
	ssize_t l;

	optf = NULL;

	opterr = 0;
	while ((c = getopt(argc, argv, "qrwd")) != -1) {
		switch (c) {
			case 'r': ifopt; opt = OPT_READ; break;
			case 'w': ifopt; opt = OPT_WRITE; break;
			case 'd': ifopt; opt = OPT_REMOVE; break;
			case 'q': quiet = 1; break;
			default: usage(); break;
		}
	}

	optf = argv[optind];
	if (!optf) usage();

	/* sanity checks */
	if (!check_secure_path(optf)) {
		errno = EACCES;
		xerror(optf);
	}

	snprintf(data, sizeof(data), "%s/%s", CONFTOOL_BASE, optf);

	if (opt == OPT_REMOVE) {
		if (unlink(data) == -1) xerror(data);
		return 0;
	}

	fd = open(data, opt == OPT_WRITE ? O_WRONLY | O_TRUNC | O_CREAT : O_RDONLY, 0666);
	if (fd == -1) xerror(data);
	memset(data, 0, sizeof(data));

	if (!opt || opt == OPT_READ) {
		l = read(fd, data, sizeof(data));
		if (l) write(1, data, l);
	}
	else if (opt == OPT_WRITE) {
		x = 1; l = 0;
		if (argv[optind+x]) {
			while (argv[optind+x]) {
				l += strlen(argv[optind+x]);
				xstrlcat(data, argv[optind+x], sizeof(data)-l);
				l++;
				xstrlcat(data, " ", sizeof(data)-l);
				x++;
			}
			data[l-1] = '\n';
			write(fd, data, l);
		}
	}

	close(fd);
	return 0;
}
