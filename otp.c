/*
 *	Simple One-time pad implementation
 *
 *	This code (if released) is to be placed in public domain.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define BUFSIZE 4096

void usage(void)
{
	fprintf(stdout, "usage: otp <message file> [pad file] [output file]\n");
	fprintf(stdout, "message file is mandatory, pad file by default reads from stdout,\n");
	fprintf(stdout, "so you can use constructs like:\n\n  otp msg < pad > crypted\n\n");
	fprintf(stdout, "WARNING: message file and pad file must be equal in their sizes!\n");
	exit(1);
}

void xerror(char *reason)
{
	if (errno && reason)
		perror(reason);
	usage();
}

int main(int argc, char **argv)
{
	FILE *msg = NULL, *pad = stdin, *out = stdout;
	char c = 0;
	char msgbuf[BUFSIZE] = {0}, padbuf[BUFSIZE] = {0};
	int len = 0;
	int i;

	if (argc < 2) usage();

	msg = fopen(argv[1], "rb");
	if (!strcmp(argv[1], "-")) msg = stdin;
	if (!msg) xerror("open msg");

	if (argc >= 3) {
		pad = fopen(argv[2], "rb");
		if (!pad) xerror("open pad");
	}

	if (argc >= 4) {
		out = fopen(argv[3], "rb+");
		if (!out) {
			out = fopen(argv[3], "wb");
			if (!out) xerror("open output");
		}
		fseek(out, 0L, SEEK_SET);
	}

	while (1) {
		if (feof(msg)) goto _out;
		if (feof(pad)) goto _out;
		len = fread(msgbuf, 1, BUFSIZE, msg);
		fread(padbuf, 1, len, pad);
		for (i = 0; i < len; i++)
// Here is core of all that program: it XORs msgbuf with padbuf
			msgbuf[i] ^= padbuf[i];
		if (fwrite(msgbuf, 1, len, out) < len) xerror("write");
		if (fflush(out)) xerror("flush");
	}

_out:
	fclose(msg);
	fclose(pad);
	fclose(out);
	return 0;
}
