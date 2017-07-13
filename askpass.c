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
#include <errno.h>

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
#define GETP_WAITFILL 4

struct getpasswd_state;
struct termios;

typedef int (*getpasswd_filt)(struct getpasswd_state *, int, size_t);

struct getpasswd_state {
	char *passwd;
	size_t pwlen;
	const char *echo;
	char maskchar;
	getpasswd_filt charfilter;
	int fd;
	int efd;
	int error;
	struct termios *sanetty;
	int flags;
	size_t retn;
};

size_t s_getpasswd(struct getpasswd_state *getps)
{
	char c;
	int tty_opened = 0, x;
	int clear;
	struct termios s, t;
	size_t l, echolen = 0;

	if (!getps) return ((size_t)-1);

	/*
	 * Both stdin and stderr point to same fd. This cannot happen.
	 * This only means that getps was memzero'd.
	 * Do not blame user for that, just fix it.
	 */
	if ((getps->fd == 0 && getps->efd == 0) || getps->efd == -1) getps->efd = 2;

	if (getps->fd == -1) {
		if ((getps->fd = open("/dev/tty", O_RDONLY|O_NOCTTY)) == -1) getps->fd = 0;
		else tty_opened = 1;
	}

	memset(&t, 0, sizeof(struct termios));
	memset(&s, 0, sizeof(struct termios));
	if (tcgetattr(getps->fd, &t) == -1) {
		getps->error = errno;
		return ((size_t)-1);
	}
	s = t;
	if (getps->sanetty) memcpy(getps->sanetty, &s, sizeof(struct termios));
	cfmakeraw(&t);
	t.c_iflag |= ICRNL;
	if (tcsetattr(getps->fd, TCSANOW, &t) == -1) {
		getps->error = errno;
		return ((size_t)-1);
	}

	if (getps->echo) {
		echolen = strlen(getps->echo);
		if (write(getps->efd, getps->echo, echolen) == -1) {
			getps->error = errno;
			l = ((size_t)-1);
			goto _xerr;
		}
	}

	l = 0; x = 0;
	memset(getps->passwd, 0, getps->pwlen);
	while (1) {
		clear = 1;
		if (read(getps->fd, &c, sizeof(char)) == -1) {
			getps->error = errno;
			l = ((size_t)-1);
			goto _xerr;
		}
		if (getps->charfilter) {
			x = getps->charfilter(getps, c, l);
			if (x == 0) {
				clear = 0;
				goto _newl;
			}
			else if (x == 2) continue;
			else if (x == 3) goto _erase;
			else if (x == 4) goto _delete;
			else if (x == 5) break;
			else if (x == 6) {
				clear = 0;
				l = getps->retn;
				memset(getps->passwd, 0, getps->pwlen);
				goto _err;
			}
		}
		if (l >= getps->pwlen && (getps->flags & GETP_WAITFILL)) clear = 0;

		if (c == '\x7f'
		|| (c == '\x08' && !(getps->flags & GETP_NOINTERP))) { /* Backspace / ^H */
_erase:			if (l == 0) continue;
			clear = 0;
			l--;
			if (!(getps->flags & GETP_NOECHO)) {
				if (write(getps->efd, "\x08\033[1X", sizeof("\x08\033[1X")-1) == -1) {
					getps->error = errno;
					l = ((size_t)-1);
					goto _xerr;
				}
			}
		}
		else if (!(getps->flags & GETP_NOINTERP)
		&& (c == '\x15' || c == '\x17')) { /* ^U / ^W */
_delete:		clear = 0;
			l = 0;
			memset(getps->passwd, 0, getps->pwlen);
			if (write(getps->efd, "\033[2K\033[0G", sizeof("\033[2K\033[0G")-1) == -1) {
				getps->error = errno;
				l = ((size_t)-1);
				goto _xerr;
			}
			if (getps->echo) {
				if (write(getps->efd, getps->echo, echolen) == -1) {
					getps->error = errno;
					l = ((size_t)-1);
					goto _xerr;
				}
			}
		}
_newl:		if (c == '\n'
		|| c == '\r'
		|| (!(getps->flags & GETP_NOINTERP) && c == '\x04')) break;
		if (clear) {
			*(getps->passwd+l) = c;
			l++;
			if (!(getps->flags & GETP_NOECHO)) {
				if (getps->maskchar &&
					write(getps->efd, &getps->maskchar,
					sizeof(char)) == -1) {
						getps->error = errno;
						l = ((size_t)-1);
						goto _xerr;
				}
			}
		}
		if (l >= getps->pwlen && !(getps->flags & GETP_WAITFILL)) break;
	};

_err:	if (write(getps->efd, "\r\n", sizeof("\r\n")-1) == -1) {
		getps->error = errno;
		l = ((size_t)-1);
	}
	if (x != 6) *(getps->passwd+l) = 0;

_xerr:	if (tcsetattr(getps->fd, TCSANOW, &s) == -1) {
		if (getps->error == 0) {
			getps->error = errno;
			l = ((size_t)-1);
		}
	}

	if (tty_opened) close(getps->fd);

	return l;
}

void xgetpasswd(char *password, size_t pwdlen, const char *fmt, ...)
{
	static char prompt[256];
	struct getpasswd_state getps;
	va_list ap;

	va_start(ap, fmt);

	memset(prompt, 0, sizeof(prompt));
	vsnprintf(prompt, sizeof(prompt), fmt, ap);

	memset(&getps, 0, sizeof(struct getpasswd_state));
	getps.fd = getps.efd = -1;
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
	xgetpasswd(pw, sizeof(pw)-1, "%s:", prompt);

	r = match_password(usr, pw) ? 0 : 1; /* shell true/false */
	memset(pw, 0, sizeof(pw));

	return r;
}
