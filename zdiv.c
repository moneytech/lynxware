#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char **argv)
{
	uint64_t d, y, x;

	if (argc < 3) return 1;

	d = atoll(*(argv+1));
	y = atoll(*(argv+2));

	for (x = 2; x < d; x++)
		if (!(y % x)) printf("%lld\n", x);

	return 0;
}
