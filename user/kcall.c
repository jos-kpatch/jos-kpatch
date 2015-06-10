#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;

	if (argc != 2) {
		printf("Usage: %s <function name>\n", argv[0]);
		exit();
	}

	r = sys_call_function(argv[1]);
	if (r < 0)
		printf("sys_call_function: %e\n", r);
}
