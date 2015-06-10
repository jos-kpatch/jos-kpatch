#include <inc/stdio.h>

extern int kpatch_test1(void);

void
test2(void)
{
	cprintf("Hello world\n");
}

int
module_init(void)
{
	kpatch_test1();
	cprintf("test2 address: %p\n", test2);

	return 0;
}
