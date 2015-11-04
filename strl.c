#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	int x;
	char B[4096];
	size_t l = 0;

	if (*(argv+1)) {
		for (x = 1; *(argv+x); x++) l += strlen(*(argv+x)); l += x-2;
		printf("%zu\n", l);
	}
	else {
		while (1) {
			fgets(B, sizeof(B), stdin);
			if (!*(B)) break;
			l = strlen(B); l--;
			printf("%zu\n", l);
		}
	}

	return 0;
}
