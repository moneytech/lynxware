#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

static int s_isnum(const char *s)
{
	char *p;
	if (!s || *s == 0) return 0;
	strtol(s, &p, 10);
	return (*p == 0);
}

int main(int argc, char **argv)
{
	argv += 1;
	while (*argv) {
		if (!s_isnum(*argv)) break;
		signal(atoi(*argv), SIG_IGN);
		argv++;
	}

	execvp(*argv, argv);
	exit(127);
}
