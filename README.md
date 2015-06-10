# Kpatch for JOS

We implemented

1. Patching a kernel-function with another kernel-function. See kern/kpatch.{h,c}.
2. Loadable kernel module (that workds only one-time). See modules/ and user/insmod.c.
3. User-space utilities. See user/kcall.c, user/kpatch.c and user/insmod.c.

