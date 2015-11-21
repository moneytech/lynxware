#include <stdio.h>

int main(int argc, char **argv)
{
	FILE *s = stdin, *d = stdout;
	int c;

	if (*(argv+1)) s = fopen(*(argv+1), "rb");
	if (!s) return 1;

	while (1) {
		if (feof(s) || ferror(s)) break;
		c = fgetc(s);
		if (c == EOF) break;
		fprintf(d, "%02hhx", c);
		fflush(d);
	}

	return 0;
}
