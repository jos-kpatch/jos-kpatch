#include <inc/lib.h>

void
umain(int argc, char **argv)
{
	int r;

	if (argc != 3) {
		printf("Usage: %s <function name> <new address>\n", argv[0]);
		exit();
	}

	r = sys_patch_function(argv[1], (void *) strtol(argv[2], 0, 0));
	if (r < 0)
		printf("sys_patch_function: %e\n", r);
}
