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

#define GETP_NOECHO 1
#define GETP_NOINTERP 2

struct getpasswd_state;

typedef int (*getpasswd_filt_t)(struct getpasswd_state *, int, size_t);

struct getpasswd_state {
	char *passwd;
	size_t pwlen;
	const char *echo;
	int maskchar;
	getpasswd_filt_t charfilter;
	int fd;
	int flags;
};

size_t s_getpasswd(struct getpasswd_state *getps)
{
	int fd;
	int c, clear;
	struct termios s, t;
	size_t l;

	if (!getps) return ((size_t)-1);

	if (getps->fd == -1) {
		if ((fd = open("/dev/tty", O_RDONLY|O_NOCTTY)) < 0) fd = 0;
	}
	else fd = getps->fd;

	memset(&t, 0, sizeof(struct termios));
	memset(&s, 0, sizeof(struct termios));
	tcgetattr(fd, &t);
	s = t;
	cfmakeraw(&t);
	t.c_iflag |= ICRNL;
	tcsetattr(fd, TCSANOW, &t);

	if (getps->echo) {
		fputs(getps->echo, stderr);
		fflush(stderr);
	}

	l = 0;
	while (1) {
		clear = 1;
		c = 0;
		if (read(fd, &c, sizeof(char)) == -1) break;
		if (getps->charfilter) {
			int x = getps->charfilter(getps, c, l);
			if (x == 0) {
				clear = 0;
				goto _newl;
			}
			else if (x == 2) continue;
			else if (x == 3) goto _erase;
			else if (x == 4) goto _delete;
			else if (x == 5) break;
		}

		if (c == '\x7f'
		|| (c == '\x08' && !(getps->flags & GETP_NOINTERP))) { /* Backspace / ^H */
_erase:			if (l == 0) continue;
			clear = 0;
			l--;
			if (!(getps->flags & GETP_NOECHO)) fputs("\x08\e[1X", stderr);
			fflush(stderr);
		}
		else if (!(getps->flags & GETP_NOINTERP)
		&& (c == '\x15' || c == '\x17')) { /* ^U / ^W */
_delete:		clear = 0;
			l = 0;
			memset(getps->passwd, 0, getps->pwlen);
			fputs("\e[2K\e[0G", stderr);
			if (getps->echo) fputs(getps->echo, stderr);
			fflush(stderr);
		}
_newl:		if (c == '\n' || c == '\r' || (!(getps->flags & GETP_NOINTERP) && c == '\x04')) break;
		if (clear) {
			*(getps->passwd+l) = c;
			l++;
			if (!(getps->flags & GETP_NOECHO)) fputc(getps->maskchar, stderr);
			fflush(stderr);
		}
		if (l >= getps->pwlen) break;
	};

	fputs("\r\n", stderr);
	fflush(stderr);
	*(getps->passwd+l) = 0;

	tcsetattr(fd, TCSANOW, &s);

	if (fd > 2 && getps->fd == -1) close(fd);

	return l;
}


void getpasswd(char *password, size_t pwdlen, const char *fmt, ...)
{
	static char prompt[256];
	struct getpasswd_state getps;
	va_list ap;

	va_start(ap, fmt);

	memset(prompt, 0, sizeof(prompt));
	vsnprintf(prompt, sizeof(prompt), fmt, ap);

	memset(&getps, 0, sizeof(struct getpasswd_state));
	getps.fd = -1;
	getps.passwd = password;
	getps.pwlen = pwdlen;
	getps.echo = prompt;
	getps.flags = GETP_NOECHO;

	s_getpasswd(&getps);

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
