#ifndef JOS_INC_KPATCH_H
#define JOS_INC_KPATCH_H

int kpatch_patch_function(void *old, void *new);
int kpatch_patch_function_with_name(const char *function_name, void *new);

#endif // !JOS_INC_KPATCH_H
