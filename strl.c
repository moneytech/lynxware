#include <stdio.h>
#include <unistd.h>
#include <string.h>

static char B[65536];

int main(int argc, char **argv)
{
	int x;
	size_t l;

	if (*(argv+1)) {
		for (l = 0, x = 1; *(argv+x); x++) l += strlen(*(argv+x));
		l += x-2;
		printf("%zu\n", l);
	}
	else {
		while (1) {
			l = 0;
			fgets(B, sizeof(B)-1, stdin);
			if (!*(B)) break;
			l = strlen(B);
			if (l && *(B+l-1) == '\n') l--;
			printf("%zu\n", l);
			if (l == 0) break;
		}
	}

	return 0;
}
