/*
 *	dumpmem: dumps memory of process' virtual address space
 *
 *	 usage: dumpmem [-i addr] [-x addr] [-s] [-o outfile] pid
 *		-i addr: include only those address ranges in output
 *		-x addr: dump everything except those address ranges
 *		-s: dump only executable and it's mappings
 *		-o outfile: write output to outfile instead of stdout
 *		pid: process id to dump memory from
 *
 *	Not portable: is Linux (or ptrace()) specific.
 */

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <libgen.h>

#define PAGE_SIZE 0x1000

#define LINUX_USER_ADDR32 "0xb7000000-0xb7ffffff"
#define LINUX_KERNEL_ADDR32 "0xfff80000-0xffffffff"
#define LINUX_USER_ADDR64 "0x7f0000000000-0x7fffffffffff"
#define LINUX_KERNEL_ADDR64 "0xffffffff80000000-0xffffffffffffffff"

static char *progname;

static char tmpname[PATH_MAX];
static uint8_t buf[PAGE_SIZE];
static int fd, ofd;
static uint64_t *ignaddrs; static int ignaddrl;
static uint64_t *goodaddrs; static int goodaddrl;

static void freedata(void)
{
	if (fd != -1) close(fd);
	if (ofd != -1) close(ofd);
	if (ignaddrs) free(ignaddrs);
	if (goodaddrs) free(goodaddrs);
}

static void xerror(const char *reason)
{
	char buf[256] = {0};

	snprintf(buf, sizeof(buf), "%s: %s", progname, reason);
	if (errno && reason)
		perror(buf);
	else
		fprintf(stderr, "%s\n", buf);

	freedata();
	exit(1);
}

static void parse_maps(pid_t pid, int line, uint64_t *start, uint64_t *end)
{
	int mfd, cline = 0;
	ssize_t l;
	char c, *p = buf, *d;

	snprintf(tmpname, sizeof(tmpname), "/proc/%u/maps", pid);
	mfd = open(tmpname, O_RDONLY);
	if (mfd == -1) goto _err;

	while (cline < line) {
		l = read(mfd, &c, sizeof(char));
		if (!l || l == -1) break;
		if (c == '\n') cline++;
	}

	while (c != ' ') {
		l = read(mfd, &c, sizeof(char));
		if (!l || l == -1) break;
		if (c == '\n') goto _err;
		*p++ = c;
	}
	*(p-1) = 0;

	p = strchr(buf, '-');
	if (p) { *p = 0; p++; }
	else goto _err;

	*start = (uint64_t)strtoull(buf, &d, 16);
	if (*d) goto _err;
	*end = (uint64_t)strtoull(p, &d, 16);
	if (*d) goto _err;

	if (mfd != -1) close(mfd);
	return;

_err:
	if (mfd != -1) close(mfd);

	*start = 0;
	*end = 0;
}

static int allowaddr(const char *saddr)
{
	char tmp[256], *p, *d;

	if (!goodaddrs) goodaddrl = 0;
	goodaddrl += 2;
	goodaddrs = realloc(goodaddrs, goodaddrl * sizeof(uint64_t));

	memcpy(tmp, saddr, sizeof(tmp));
	p = strchr(tmp, '-');
	if (p) { *p = 0; p++; }
	else goto _err;

	goodaddrs[goodaddrl-2] = (uint64_t)strtoull(tmp, &d, 16);
	if (*d) goto _err;
	goodaddrs[goodaddrl-1] = (uint64_t)strtoull(p, &d, 16);
	if (*d) goto _err;
	if (goodaddrs[goodaddrl-1] < goodaddrs[goodaddrl-2]) goto _err;
	return 1;

_err:
	goodaddrl -= 2;
	goodaddrs = realloc(goodaddrs, goodaddrl * sizeof(uint64_t));
	return 0;
}

static int ignoreaddr(const char *saddr)
{
	char tmp[256], *p, *d;

	if (!ignaddrs) ignaddrl = 0;
	ignaddrl += 2;
	ignaddrs = realloc(ignaddrs, ignaddrl * sizeof(uint64_t));

	memcpy(tmp, saddr, sizeof(tmp));
	p = strchr(tmp, '-');
	if (p) { *p = 0; p++; }
	else goto _err;

	ignaddrs[ignaddrl-2] = (uint64_t)strtoull(tmp, &d, 16);
	if (*d) goto _err;
	ignaddrs[ignaddrl-1] = (uint64_t)strtoull(p, &d, 16);
	if (*d) goto _err;
	if (ignaddrs[ignaddrl-1] < ignaddrs[ignaddrl-2]) goto _err;
	return 1;

_err:
	ignaddrl -= 2;
	ignaddrs = realloc(ignaddrs, ignaddrl * sizeof(uint64_t));
	return 0;
}

static int ignoredrange(uint64_t s, uint64_t e)
{
	int i, ret = 0;

	for (i = 0; i < ignaddrl; i++) {
		if (s >= ignaddrs[i] && e <= ignaddrs[i+1]) { ret = 1; return ret; }
		else ret = 0;
	}

	return ret;
}

static int allowedrange(uint64_t s, uint64_t e)
{
	int i, ret = 1;

	for (i = 0; i < goodaddrl; i++) {
		if (s >= goodaddrs[i] && e <= goodaddrs[i+1]) { ret = 1; return ret; }
		else ret = 0;
	}

	return ret;
}


int main(int argc, char **argv)
{
	progname = basename(argv[0]);

	pid_t pid;
	ssize_t l;
	uint64_t s, e;
	int pg;
	char *d, *oname = NULL;

	char c;
	opterr = 0;

	ofd = 1;

	while ((c = getopt(argc, argv, "x:i:so:")) != -1) {
		switch (c) {
			case 'x':
				if (!ignoreaddr(optarg)) xerror("Invalid address range");
				break;
			case 'i':
				if (!allowaddr(optarg)) xerror("Invalid address range");
				break;
			case 's':
				ignoreaddr(LINUX_USER_ADDR32);
				ignoreaddr(LINUX_USER_ADDR64);
				ignoreaddr(LINUX_KERNEL_ADDR32);
				ignoreaddr(LINUX_KERNEL_ADDR64);
				break;
			case 'o':
				oname = optarg;
				break;
			default:
				break;
		}
	}

	if (argv[optind]) {
		pid = strtol(argv[optind], &d, 10);
		if (*d) xerror("Invalid pid");
	}
	else xerror("No pid given!");

	snprintf(tmpname, sizeof(tmpname), "/proc/%u/mem", pid);
	fd = open(tmpname, O_RDONLY);
	if (fd == -1) xerror("open");

	if (oname) {
		ofd = open(oname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if (ofd == -1) xerror("output");
	}

	pg = 0; s = 0; e = 0;
_next:
	parse_maps(pid, pg, &s, &e);
	if (!s || !e) goto _out;
	if (ignoredrange(s, e) || !allowedrange(s, e)) { pg++; goto _next; }
	lseek(fd, s, SEEK_SET);

	while (1) {
		l = read(fd, buf, sizeof(buf));
		if (!l || l == -1 || l < sizeof(buf) || s >= e) {
			memset(buf, 0, sizeof(buf));
			if (l == -1 && s < e)
				fprintf(stderr,
					"unreadable region: 0x%016llx-0x%016llx\n",
					s, e);
			while (s < e) {
				if (write(ofd, buf, sizeof(buf)) == -1) break;
				s += sizeof(buf);
			}
			pg++;
			goto _next;
		}
		s += l;
		if (write(ofd, buf, l) == -1) break;
	}

_out:
	close(fd);

	freedata();
	return 0;
}
