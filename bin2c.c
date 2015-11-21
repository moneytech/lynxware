#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static char b[4092];

static void xerror(const char *s)
{
	perror(s);
	exit(1);
}

static void underscore(char *s)
{
	while (*s) {
		if (*s == '/' || *s == '\\' || *s == '.') *s = '_';
		s++;
	}
}

int main(int argc, char **argv)
{
	FILE *f = stdin;
	char *fs = "_stdin";
	size_t l;
	int x;

	if (*(argv+1) && strcmp(*(argv+1), "-")) {
		fs = *(argv+1);
		f = fopen(fs, "rb");
		if (!f) xerror(fs);
	}

	underscore(fs);
	printf("unsigned char %s[] = {", fs);
	while (1) {
		if (feof(f)) {
			if (!(x%12)) printf("\n\t");
			printf("0x%02hhx", (unsigned char)*(b+x));
			break;
		}
		l = fread(b, 1, sizeof(b), f);
		if (!l) {
			printf("\n\t/* empty */");
			break;
		}
		if (ferror(f)) xerror(fs);
		feof(f) ? l-- : l;
		for (x = 0; x < l; x++) {
			if (!(x%12)) printf("\n\t");
			printf("0x%02hhx, ", (unsigned char)*(b+x));
		}
	}
	printf("\n};\n");

	return 0;
}
