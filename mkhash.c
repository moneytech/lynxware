/*
 *	New mkhash.c, rewritten.
 *	This code is placed into public domain.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <crypt.h>
#include <termios.h>
#include <fcntl.h>

#define SALTLEN 32
#define PASSLEN 256

/* Thanks to musl for this code */
static void getpasswd(char *password, const char *echo, int pwdlen)
{
	int fd;
	struct termios s, t;
	ssize_t l;

	if ((fd = open("/dev/tty", O_RDONLY|O_NOCTTY)) < 0) fd = 0;

	tcgetattr(fd, &t);
	s = t;
	t.c_lflag &= ~ECHO;
	t.c_lflag |= ICANON;
	t.c_iflag &= ~(INLCR|IGNCR);
	t.c_iflag |= ICRNL;
	tcsetattr(fd, TCSAFLUSH, &t);
	tcdrain(fd);

	fputs(echo, stderr);
	fflush(stderr);

	l = read(fd, password, pwdlen);
	if (l >= 0) {
		if (l > 0 && password[l-1] == '\n') l--;
		password[l] = 0;
	}

	fputc('\n', stderr);
	fflush(stderr);

	tcsetattr(fd, TCSAFLUSH, &s);

	if (fd > 2) close(fd);
}

static void getstring(char *out, const char *echo, int len)
{
	int fd;
	ssize_t l;

	fputs(echo, stderr);
	fflush(stderr);

	if ((fd = open("/dev/tty", O_RDONLY|O_NOCTTY)) < 0) fd = 0;

	l = read(fd, out, len);
	if (l >= 0) {
		if (l > 0 && out[l-1] == '\n') l--;
		out[l] = 0;
	}

	if (fd > 2) close(fd);
}

int main(int argc, char **argv)
{
	char salt[SALTLEN] = {0};
	char pass[PASSLEN] = {0};
	char *hash;
	size_t l;

	if (*(argv+1) && !strcmp(*(argv+1), "-R")) {
		printf("Enter salt:");
		fflush(stdout);
		l = read(0, salt, SALTLEN);
		salt[l] = 0;
		printf("Enter password:");
		fflush(stdout);
		l = read(0, pass, PASSLEN);
		pass[l] = 0;
	}
	else {
		getstring(salt, "Enter salt:", SALTLEN);
		getpasswd(pass, "Enter password:", PASSLEN);
	}

	hash = crypt(pass, salt);
	if (!hash) goto _err;
	fprintf(stdout, "%s\n", hash);

	memset(pass, 0, sizeof(pass));
	memset(salt, 0, sizeof(salt));

	return 0;

_err:
	return 1;
}
