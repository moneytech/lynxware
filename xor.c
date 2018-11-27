#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define NOSIZE ((size_t)-1)

#define DATASIZE 65536

static char ublkx[DATASIZE];
static char ublky[DATASIZE];
static size_t *sblkx = (size_t *)ublkx;
static size_t *sblky = (size_t *)ublky;

static void usage(void)
{
	puts("usage: xor FILEX FILEY [RESULT]");
	puts("XOR contents of FILEX into FILEY,");
	puts("then write result to stdout.");
	puts("If RESULT is given as a file name,");
	puts("then write result to that file instead.");
	exit(0);
}

static void xerror(const char *s)
{
	if (s) perror(s);
	exit(2);
}

static int will_exit;

int main(int argc, char **argv)
{
	int ifdx, ifdy, ofd, pfd;
	char *pblk;
	char *infnamex, *infnamey, *onfname;
	size_t lio, lrem, ldone, lblock, ldata;
	size_t shft, z, x;

	if (argc < 3) usage();
	infnamex = argv[1];
	infnamey = argv[2];
	if (argc >= 4) onfname = argv[3];
	else onfname = "-";

	if (!strcmp(infnamex, "-")) ifdx = 0;
	else {
		ifdx = open(infnamex, O_RDONLY);
		if (ifdx == -1) xerror(infnamex);
	}
	if (!strcmp(infnamey, "-")) ifdy = 0;
	else {
		ifdy = open(infnamey, O_RDONLY);
		if (ifdy == -1) xerror(infnamey);
	}
	if (!strcmp(onfname, "-")) ofd = 1;
	else {
		ofd = creat(onfname, 0666);
		if (ofd == -1) xerror(onfname);
	}

	switch (sizeof(size_t)) {
		case 2: shft = 1; break;
		case 4: shft = 2; break;
		case 8: shft = 3; break;
		default: xerror(NULL); break;
	}

	will_exit = 0;
	ldata = DATASIZE;
	while (1) {
		if (will_exit) break;
		pfd = ifdx;
		pblk = ublkx;
_nextblk:	ldone = 0;
		lrem = lblock = ldata;
_ragain:	lio = read(pfd, pblk, lrem);
		if (lio == 0) will_exit = 1;
		if (lio != NOSIZE) ldone += lio;
		else xerror(infnamex);
		if (lio && lio < lrem) {
			pblk += lio;
			lrem -= lio;
			goto _ragain;
		}

		if (pfd != ifdy) {
			if (will_exit) ldata = ldone;
			pfd = ifdy;
			pblk = ublky;
			goto _nextblk;
		}

		sblkx = (size_t *)ublkx;
		sblky = (size_t *)ublky;
		for (z = 0; z < (ldone >> shft); z++) sblkx[z] ^= sblky[z];
		if (ldone - (z << shft)) for (x = (z << shft); x < ldone; x++) ublkx[x] ^= ublky[x];

		pblk = ublkx;
		lrem = ldone;
		ldone = 0;
_wagain:	lio = write(ofd, pblk, lrem);
		if (lio != NOSIZE) ldone += lio;
		else xerror(onfname);
		if (lio < lrem) {
			pblk += lio;
			lrem -= lio;
			goto _wagain;
		}
	}

	memset(ublkx, 0, DATASIZE);
	memset(ublky, 0, DATASIZE);
	close(ifdx);
	close(ifdy);
	close(ofd);

	return 0;
}
