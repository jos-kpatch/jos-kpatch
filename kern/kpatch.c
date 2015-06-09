#include <inc/stab.h>
#include <inc/stdio.h>
#include <inc/error.h>
#include <inc/string.h>

#include <kern/kdebug.h>

extern const struct Stab __STAB_BEGIN__[];	// Beginning of stabs table
extern const struct Stab __STAB_END__[];	// End of stabs table
extern const char __STABSTR_BEGIN__[];		// Beginning of string table
extern const char __STABSTR_END__[];		// End of string table

void *__fentry__;

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
		return -E_NOT_PATCHABLE;

	if (old != (void *) info.eip_fn_addr)
		return -E_NOT_PATCHABLE;

	if (new && old != new) { // patch!
		cprintf("patching function %.*s with 0x%p\n", info.eip_fn_namelen, info.eip_fn_name, new);
		*old_as_char = 0xe9; // jmp
		*((uint32_t *) (old_as_char + 1)) = new - (old + 5);
	} else { // restore!
		cprintf("restoring function %.*s with 0x%p\n", info.eip_fn_namelen, info.eip_fn_name, old);
		*old_as_char = 0x90; // jmp
		*((uint32_t *) (old_as_char + 1)) = 0x90909090;
	}

	return 0;
}

int
kpatch_patch_function_with_name(char *function_name, void *new)
{
	const struct Stab *stab;
	int function_namelen;

	function_namelen = strlen(function_name);
	for (stab = __STAB_BEGIN__; stab < __STAB_END__; stab++)
		if (stab->n_type == N_FUN &&
		    __STABSTR_BEGIN__[stab->n_strx + function_namelen] == ':' &&
		    strncmp(function_name, __STABSTR_BEGIN__ + stab->n_strx, function_namelen) == 0)
			return kpatch_patch_function((void *) stab->n_value, new);

	return -E_NO_FUNCTION;
}
