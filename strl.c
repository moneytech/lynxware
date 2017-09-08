#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	char *ln = NULL;
	int x;
	size_t l, u;

	if (*(argv+1)) {
		for (l = 0, x = 1; *(argv+x); x++) l += strlen(*(argv+x));
		l += x-2;
		printf("%zu\n", l);
	}
	else {
		while (1) {
			l = (size_t)getline(&ln, &u, stdin);
			if (l == (size_t)-1) break;
			if (l && *(ln+l-1) == '\n') l--;
			printf("%zu\n", l);
			if (l == 0) break;
		}
	}

	if (ln) memset(ln, 0, u);
	return 0;
}
