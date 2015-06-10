#ifndef JOS_INC_KPATCH_H
#define JOS_INC_KPATCH_H

#define INFO_START "\033[30m[INFO] "
#define INFO_END "\033[0m\n"

int kpatch_patch_function(void *old, void *new);
int kpatch_patch_function_with_name(const char *function_name, void *new);
int kpatch_call_function_with_name(const char *function_name);

#endif // !JOS_INC_KPATCH_H
