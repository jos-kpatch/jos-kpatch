#include <inc/lib.h>
#include <inc/elf.h>

void
umain(int argc, char **argv)
{
	if (argc != 3) {
		printf("Usage: %s <function name> <new address>\n", argv[0]);
		exit();
	}

	sys_patch_function(argv[1], (void *) strtol(argv[2], 0, 0));
}
