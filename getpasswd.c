/*
 * Modern getpasswd routine, designed for secure password input in tty.
 * Because of so many input parameters and variants, it accepts a structure with parameters.
 * Supports callback function for character filtering.
 *
 * -- Lynx, 21Jun2017.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define GETP_NOECHO 1 /* Do not echo replacement character, behave like login(1) psw. prompt. */
#define GETP_NOINTERP 2 /* Do not interpret special tty sequences like ^W/^H. */
#define GETP_WAITFILL 4 /* Do not automatically return after all required password characters are typed, but ignore any input and just wait for user's RETURN/^D. */

struct getpasswd_state; /* Needed for typedef below. */
struct termios;

/*
 * Optional password character filter, called on each typed character.
 * Parameters:
 * @(struct getpasswd_state): full getpasswd state parameters as passed to getpasswd,
 * Filter can examine state and modify it.
 * @(int): currently typed character,
 * @(size_t): current position of character in password string.
 *
 * Filter must return:
 * 0 if character must be rejected and no replacement char echoed,
 * 1 if character is valid (accept it),
 * 2 if newline character must be rejected (special value to disable newline),
 * 3 if character should be erased (BS/^H),
 * 4 if password string should be wiped, and line should be reset (^U),
 * 5 for immediate return.
 * 6 causes getpasswd to immediately return getps->retn.
 *
 * Note that any other return values not listed there are reserved.
 * Any other return value is equal to 1, but it can become meaningful in future.
 */
typedef int (*getpasswd_filt)(struct getpasswd_state *, char, size_t);

struct getpasswd_state {
	char *passwd; /* Where to store typed password, */
	size_t pwlen; /* It's maximum length, */
	const char *echo; /* Prompt string, can be NULL, */
	char maskchar; /* Replacement character. Ignored if GETP_NOECHO., */
	getpasswd_filt charfilter; /* Character filter callback function on each typed character, */
	int fd; /* fd to read from. If -1, then try to open /dev/tty and perform operations, */
	int efd; /* stderr fd to write messages to. If -1, then set and use 2. */
	int error; /* nonzero if any other error occured during operation. */
	struct termios *sanetty; /* Previous tty state before manipulations. Caller can restore from it safely. */
	int flags; /* GETP_* flags above. */
	size_t retn; /* Return value of getpasswd if charfilter returned 6. */
};

/*
 * getpasswd - ask user for password.
 * @getps: group of getpasswd parameters (see struct getpasswd_state).
 * Note that getpasswd is allowed to modify state.
 *
 * Asks user for password in a secure way. It disables any special tty sequences such as ^C or ^\
 * so asking program will not be interrupted in any way. It also can prevent any other malicious
 * user behavior who may try to overthrow the program.
 * It interprets some basic special sequences such as ^W/^H/BS/^U/^D, and they can be disabled too.
 * Returns a number of actually read characters so caller can check if password is too short.
 */
size_t xgetpasswd(struct getpasswd_state *getps)
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
