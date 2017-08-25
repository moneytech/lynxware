/* cc unclaws.c -I/path/to/claws-mail-x.xx.x/src/common -o unclaws `pkg-config --cflags --libs glib-2.0` */

#include <stdio.h>
#include <string.h>
#include "base64.c"
#include "passcrypt.c"

#include "xstrlcpy.c"

static char in[4096], out[sizeof(in)];
static int encdec;

static char *unclaws(int mode, char *pwdout, const char *pwdin, size_t szpwdin)
{
	size_t l;

	if (mode == 1) {
		if (*pwdin == '!') pwdin++;
		l = base64_decode(pwdout, pwdin, szpwdin);
		if (!l) return "<invalid>";
		passcrypt_decrypt(pwdout, l);
		*(pwdout+l) = 0;
	}
	else if (mode == 2) {
		char t[sizeof(in)], *p;

		*pwdout = '!'; p = pwdout; p++;
		memset(t, 0, sizeof(t));
		xstrlcpy(t, pwdin, szpwdin);
		passcrypt_encrypt(t, szpwdin);
		base64_encode(p, t, szpwdin);
	}
	return pwdout;
}

int main(int argc, char **argv)
{
	size_t l;

	if (!*(argv+1)) encdec = 1;
	else if (!strcmp(*(argv+1), "-e")) encdec = 2;
	else if (!strncmp(*(argv+1), "-h", 2) || !strncmp(*(argv+1), "--h", 3)) {
		printf("usage: unclaws [-e]\n");
		printf("  -e: encrypt passwords instead\n");
		printf("default is to decrypt passwords\n");
		return 0;
	}

	printf("Input any accountrc hashes you want to %s,\n", encdec == 1 ? "decrypt" : "encrypt");
	printf("with or without '!' mark. Input Ctrl-D when done.\n\n");
	while (1) {
		memset(in, 0, sizeof(in));
		memset(out, 0, sizeof(out));

		printf("> "); fflush(stdout);

		fgets(in, sizeof(in), stdin);
		if (!*in) break;
		l = strlen(in); l--; *(in+l) = 0;
		if (!l) break;
		printf("\"%s\" = \"%s\"\n", in, unclaws(encdec, out, in, l));
	}

	return 0;
}
