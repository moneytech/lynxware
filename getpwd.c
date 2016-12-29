#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

typedef void (*sighandler_t)(int);

static struct termios svt;

static void sighandl(int unused)
{
	tcsetattr(0, TCSAFLUSH, &svt);
	exit(3);
}

/* musl! */
void getpasswd(char *password, size_t pwdlen, const char *fmt, ...)
{
	int fd;
	struct termios s, t;
	ssize_t l;
	va_list ap;

	va_start(ap, fmt);

	if ((fd = open("/dev/tty", O_RDONLY|O_NOCTTY)) < 0) fd = 0;

	tcgetattr(fd, &t);
	svt = s = t;
	t.c_lflag &= ~ECHO;
	t.c_lflag |= ICANON;
	t.c_iflag &= ~(INLCR|IGNCR);
	t.c_iflag |= ICRNL;
	tcsetattr(fd, TCSAFLUSH, &t);
	tcdrain(fd);

	vfprintf(stderr, fmt, ap);
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

	va_end(ap);
}

int main(int argc, char **argv)
{
	char *prompt;
	char pw[256];
	int x;

	for (x = 1; x < NSIG; x++) signal(x, sighandl);

	memset(pw, 0, sizeof(pw));
	prompt = getenv("GETPWD_PROMPT");
	if (!prompt) prompt = "Password";
	getpasswd(pw, sizeof(pw)-1, "%s:", prompt);

	printf("%s\n", pw);

	memset(pw, 0, sizeof(pw));

	return 0;
}
