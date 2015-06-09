#include <inc/x86.h>
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/assert.h>

#include <kern/kdebug.h>

int
kpatch_patch_function(void *old, void *new)
{
	uint8_t *old_as_char, *new_as_char;
	struct Eipdebuginfo info;

	debuginfo_eip((uintptr_t) old, &info);

	old_as_char = old;
	new_as_char = new;

	// e8: call, e9: jmp, 90: nop
	if (*old_as_char != 0xe8 && *old_as_char != 0xe9 && *old_as_char != 0x90)
		return -E_INVAL;

	if (old != (void *) info.eip_fn_addr)
		return -E_INVAL;

	cprintf("patching function %.*s with 0x%p\n", info.eip_fn_namelen, info.eip_fn_name, new);

	*old_as_char = 0xe9; // jmp
	*((uint32_t *) (old_as_char + 1)) = new - (old + 5);

	return 0;
}

int
kpatch_patch_function_with_name(char *function_name, void *new)
{
	return 0;
}
