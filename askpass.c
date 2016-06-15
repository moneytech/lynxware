#define _BSD_SOURCE

/* I suppose you have these. */
#define HAVE_CRYPT_H
#define SHADOW_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif
#ifdef SHADOW_SUPPORT
#include <shadow.h>
#endif
#include <termios.h>
#include <signal.h>

typedef void (*sighandler_t)(int);

static struct termios svt;

static void sighandl(int unused)
{
	tcsetattr(0, TCSAFLUSH, &svt);
	exit(3);
}

/* taken from super almost verbatim */
static int isnum(const char *s)
{
	char *p;
	if (!s || *s == 0) return 0;
	strtol(s, &p, 10);
	return (*p == 0);
}

#define _SUID_MAX 22
#define NOUID ((uid_t)-1)

static char *namebyuid(uid_t uid)
{
	struct passwd *p;
	char *r;

	p = getpwuid(uid);
	if (p) return strdup(p->pw_name);
	else {
		r = malloc(_SUID_MAX);
		snprintf(r, _SUID_MAX, "%u", uid);
		return r;
	}
}

static uid_t uidbyname(const char *name)
{
	struct passwd *p;

	if (isnum(name))
		return (uid_t)atoi(name);
	p = getpwnam(name);
	if (p) return p->pw_uid;
	else {
		return NOUID;
	}
}

static int match_password(const char *user, const char *secret)
{
	const char *hash;
	struct passwd *pw;
#ifdef SHADOW_SUPPORT
	struct spwd *p;
#endif

#ifdef SHADOW_SUPPORT
	p = getspnam(user);
	if (!p) goto _pw;
	hash = p->sp_pwdp;
	if (!hash) return 0;
	if (!strlen(hash) && !strlen(secret)) return 1;
	if (!strcmp(crypt(secret, hash), hash)) return 1;
_pw:
#endif
	pw = getpwnam(user);
	if (!pw) return 0;
	hash = pw->pw_passwd;
	if (!hash) return 0;
	if (!strlen(hash) && !strlen(secret)) return 1;
	if (!strcmp(crypt(secret, hash), hash)) return 1;

	return 0;
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
	char *usr = NULL, *prompt;
	char pw[256];
	int r, c;

	for (r = 1; r < NSIG; r++) signal(r, sighandl);

	while ((c = getopt(argc, argv, "u:")) != -1) {
		switch (c) {
			case 'u': usr = optarg; break;
			default: exit(4); break;
		}
	}

	if (!usr) usr = namebyuid(getuid());
	else usr = namebyuid(uidbyname(usr));
	if (!usr) return 2; /* false */

	memset(pw, 0, sizeof(pw));
	prompt = getenv("ASKPASS_PROMPT");
	if (!prompt) prompt = "Password";
	getpasswd(pw, sizeof(pw)-1, "%s:", prompt);

	r = match_password(usr, pw) ? 0 : 1; /* shell true/false */
	memset(pw, 0, sizeof(pw));

	return r;
}
