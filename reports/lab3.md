title: Lab 3
author: Leedy

# Lab 3

## Part A: User Environments and Exception Handling

### Environment State

在 JOS 中，Env 结构体就是我们在操统课上讲的 PCB，包括保存的寄存器、进程号、父进程号、进程状态、页目录指针等。JOS 中使用链表将它们串联起来。

### Allocating the Environments Array

> Exercise 1. Modify `mem_init()` in `kern/pmap.c` to allocate and map the `envs` array. This array consists of exactly NENV instances of the Env structure allocated much like how you allocated the pages array. Also like the pages array, the memory backing `envs` should also be mapped user read-only at UENVS (defined in `inc/memlayout.h`) so user processes can read from this array.
>
> You should run your code and make sure `check_kern_pgdir()` succeeds.

在 `pmap.c` 中用 `boot_alloc` 分配 `NENV * sizeof(struct Env)` 大小的内存给 `envs` 即可。`envs` 的初始化在下一节。

### Creating and Running Environments

由于需要加载 user 的程序，而到目前为止我们还没有实现文件系统，因此暂时将用户程序包在 kernel 里。这里实现的代码在 `kern/Makefrag` 84行。`_binary_obj_user_hello_start` 这些符号是链接器自动生成的。

> Exercise 2. In the file env.c, finish coding the following functions:
> 
> `env_init()`
>
> Initialize all of the Env structures in the envs array and add them to the env_free_list. Also calls env_init_percpu, which configures the segmentation hardware with separate segments for privilege level 0 (kernel) and privilege level 3 (user).
>
> `env_setup_vm()`
>
> Allocate a page directory for a new environment and initialize the kernel portion of the new environment's address space.
>
> `region_alloc()`
>
> Allocates and maps physical memory for an environment
>
> `load_icode()`
>
> You will need to parse an ELF binary image, much like the boot loader already does, and load its contents into the user address space of a new environment.
>
> `env_create()`
>
> Allocate an environment with env_alloc and call load_icode load an ELF binary into it.
>
> `env_run()`
>
> Start a given environment running in user mode.
> As you write these functions, you might find the new cprintf verb %e useful -- it prints a description corresponding to an error code. For example,
> 
> 		r = -E_NO_MEM;
> 		panic("env_alloc: %e", r);
>
> will panic with the message "env_alloc: out of memory".

这里的调用关系是（从 `i386_init` 开始）

* `env_init`: 将 `envs` 与 `env_free_list` 初始化。
* `env_create`: 加载二进制文件（暂时只能调用一次）。
	* `env_alloc`: 从 `env_free_list` 读出一个 Env，并返回。
		* `env_setup_vm`: 设置 env 的页目录与页表。
	* `load_icode`: 将二进制文件（的各个段）加载到内存中。
		* `region_alloc`: 分配内存，这其实是一个 `page_alloc` 和 `page_insert` 的封装。
* `env_run`: 运行 `envs[0]` 中的程序。

其中，`load_icode` 和 `region_alloc` 需要格外注意

1. `region_alloc` 需要注意对齐。由于这种分配是多了不碍事，因此将开始地址 ROUNDDOWN，结束地址 ROUNDUP。
2. `load_icode` 代码大部分可以参考 `boot/main.c` 中的代码，但加载代码不是用 `readseg` 而是用 `memcpy`，最后也不是跳到 `elfhdr->e_entry` 而是将 `elfhdr->e_entry` 赋值给 `e->env_tf.tf_eip`。
3. `load_icode` 之前要将 cr3 置为 `e->env_pgdir`，之后要重新置为 `kern_pgdir`。
4. `lcr3` 接收的参数一定要是物理地址，因此要用 `PADDR` 转换！不然也会报类型错误。
5. 一开始我没有区分 `ph->p_filesz` 和 `ph->p_memsz` 后来才发现是有区别的，需要用 `memset` 将多余的内存空间清零。

还有一个地方，就是为什么 `env_pop_tf` 之后 CPU 就从 Ring 0 切换到 Ring 3 了，这个其实在 `env_alloc` 里有

```c
// Set up appropriate initial values for the segment registers.
// GD_UD is the user data segment selector in the GDT, and
// GD_UT is the user text segment selector (see inc/memlayout.h).
// The low 2 bits of each segment register contains the
// Requestor Privilege Level (RPL); 3 means user mode.  When
// we switch privilege levels, the hardware does various
// checks involving the RPL and the Descriptor Privilege Level
// (DPL) stored in the descriptors themselves.
e->env_tf.tf_ds = GD_UD | 3;
e->env_tf.tf_es = GD_UD | 3;
e->env_tf.tf_ss = GD_UD | 3;
e->env_tf.tf_esp = USTACKTOP;
e->env_tf.tf_cs = GD_UT | 3;
```

到这里我看到「Once you are done you should compile your kernel and run it under QEMU」于是满怀幸喜地 `make qemu`，结果居然开始死循环重启，而且完全没有打印出任何东西……原因就是因为 printf 需要系统调用才能打印，而我们暂时还没有配置。

### Handling Interrupts and Exceptions

> **Exercise 3.** Read Chapter 9, Exceptions and Interrupts in the 80386 Programmer's Manual (or Chapter 5 of the IA-32 Developer's Manual), if you haven't already.

中断的种类有很多，详细的可以看[这里](http://pdosnew.csail.mit.edu/6.828/2014/readings/i386/s09_09.htm)。我们下面要用到的其实只有这张表。

```
Description                       Interrupt #   Error Code

Divide error                       0            No
Debug exceptions                   1            No
Breakpoint                         3            No
Overflow                           4            No
Bounds check                       5            No
Invalid opcode                     6            No
Coprocessor not available          7            No
System error                       8            Yes (always 0)
Coprocessor Segment Overrun        9            No
Invalid TSS                       10            Yes
Segment not present               11            Yes
Stack exception                   12            Yes
General protection fault          13            Yes
Page fault                        14            Yes
Coprocessor error                 16            No
Two-byte SW interrupt             0-255         No
```

在 Intel 的术语中，an interrupt is a protected control transfer that is caused by an asynchronous event usually external to the processor, such as notification of external device I/O activity. An exception, in contrast, is a protected control transfer caused synchronously by the currently running code, for example due to a divide by zero or an invalid memory access.

在 Protected Control Transfer 中，用到了以下两个硬件

1. IDT，中断描述符表。用于确定中断处理程序的位置。同时切换 CS/EIP 的值。其中 CS 的值包含 privilege level。
2. TSS，中文叫任务状态段。这里决定了终端时内核栈的位置，CPU 会自动将 SS、ESP、EFLAGS、CS、EIP 和 error code 压栈，这也是我们 `env_pop_tf` 能成功的原因。TSS 是一个很大的结构，可以看 `mmu.h` 中的 `struct Taskstate` 结构。

### Types of Exceptions and Interrupts

没啥好说的。

### An Example

举了个例子说明 error code 的作用：在缺页中断中，缺的哪一页是通过 error code 传递的。

### Nested Exceptions and Interrupts

如果中断是由内核发出的，不用切 privilege level，那么也就无需切换内核栈，也无需将旧的 SS 和 ESP 压栈。

### Setting Up the IDT

终于到写代码的地方了，观察 `trap.c` 可知，这个文件的结构和 `env.c`、`pmap.c` 差不多：用 `trap_init` 函数进行初始化设置，然后用 `trap` 函数来干事。

> **Exercise 4.** Edit `trapentry.S` and `trap.c` and implement the features described above. The macros `TRAPHANDLER` and `TRAPHANDLER_NOEC` in `trapentry.S` should help you, as well as the T_* defines in inc/trap.h. You will need to add an entry point in `trapentry.S` (using those macros) for each trap defined in `inc/trap.h`, and you'll have to provide `_alltraps` which the `TRAPHANDLER` macros refer to. You will also need to modify `trap_init()` to initialize the `idt` to point to each of these entry points defined in `trapentry.S`; the `SETGATE` macro will be helpful here.
> 
> Your `_alltraps` should:
> 
> 1. push values to make the stack look like a `struct Trapframe`
> 2. load `GD_KD` into `%ds` and `%es`
> 3. `pushl %esp` to pass a pointer to the Trapframe as an argument to `trap()`
> 4. `call trap` (can `trap` ever return?)
>
> Consider using the `pushal` instruction; it fits nicely with the layout of the `struct Trapframe`.
> 
> Test your trap handling code using some of the test programs in the `user` directory that cause exceptions before making any system calls, such as `user/divzero`. You should be able to get make grade to succeed on the `divzero`, `softint`, and `badsegment` tests at this point.


主要写的代码在 `trapentry.S` 里。由于已经提供了 `TRAPHANDLER` 和 `TRAPHANDLER_NOEC` 两个宏，因此对照上面一节的表就可以很快将内容填充出来。

接下来写 `_trapalls` 的代码，根据指导，可以很快的写出来。需要注意的是，`struct PushRegs` 的结构可以使用 `pushal` 直接压到栈上。

最后是写 `trap.c` 中的 `trap_init` 函数，需要写出一大堆函数原型，再使用 `SETGATE` 宏就可以了。

> *Challenge!* You probably have a lot of very similar code right now, between the lists of `TRAPHANDLER` in `trapentry.S` and their installations in `trap.c`. Clean this up. Change the macros in `trapentry.S` to automatically generate a table for `trap.c` to use. Note that you can switch between laying down code and data in the assembler by using the directives `.text` and `.data`.

这个 Challenge 比较简单，在 `trapentry.S` 里创建一个数组即可。这里需要灵活的切换 `.text` 和 `.data` 段。另外，可以将 `.global name` 删掉，因为我们不需要在其他文件中获取这个 symbol。

> **Questions**
>
> Answer the following questions in your answers-lab3.txt:
>
> 1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)
> 2. Did you have to do anything to make the user/softint program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but softint's code says int $14. Why should this produce interrupt vector 13? What happens if the kernel actually allows softint's int $14 instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

1. 为了将 trapno 压栈，如果全部指定到一个 handler，那么 trap 的类型就会丢失。
2. 因为我们在 `trap_init` 中 `SETGATE` 时将缺页中断的 Descriptor Privilege Level 指定为0，因此用户态程序不能直接调用 `int 14`，所以产生 General Protection 中断。由于缺页中断需要一个 Error Code，因此不能让用户手动触发。

## Part B: Page Faults, Breakpoints Exceptions, and System Calls

### Handling Page Faults

> **Exercise 5.** Modify trap_dispatch() to dispatch page fault exceptions to page_fault_handler(). You should now be able to get make grade to succeed on the faultread, faultreadkernel, faultwrite, and faultwritekernel tests. If any of them don't work, figure out why and fix them. Remember that you can boot JOS into a particular user program using make run-x or make run-x-nox.

在 `trap_dispatch()` 里加上一个 `switch ... case ...` 语句即可。

### The Breakpoint Exception

> **Exercise 6.** Modify trap_dispatch() to make breakpoint exceptions invoke the kernel monitor. You should now be able to get make grade to succeed on the breakpoint test.

再加一个分支即可，直接调用 `monitor(tf)`。

> *Challenge!* Modify the JOS kernel monitor so that you can 'continue' execution from the current location (e.g., after the int3, if the kernel monitor was invoked via the breakpoint exception), and so that you can single-step one instruction at a time. You will need to understand certain bits of the EFLAGS register in order to implement single-stepping.

首先要搞清楚怎么做到单步，CPU 在 EFLAGS 里提供了一位叫 `FL_TF`，可以用来开启单步。于是在 monitor 里添加一条新的指令 continue，将 `tf->tf_eflags` 的 `FL_TF` 置为1。然后在 `FL_TF` 模式下，CPU 每执行一条指令都会触发一个 debug exception，因此在 `trap_dispatcher` 里将 `T_DEBUG` 添加即可。

> Questions
>
> 1. The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the IDT (i.e., your call to SETGATE from trap_init). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?
> 2. What do you think is the point of these mechanisms, particularly in light of what the user/softint test program does?

1. 和上面的那个问题一样，我们必须将断点中断的 Descriptor Privilege Level 设置为3才可以让用户调用。
2. 这种机制的作用就是防止用户手动调用一个需要 Error Code 的中断。

### System calls

> **Exercise 7.** Add a handler in the kernel for interrupt vector T_SYSCALL. You will have to edit kern/trapentry.S and kern/trap.c's trap_init(). You also need to change trap_dispatch() to handle the system call interrupt by calling syscall() (defined in kern/syscall.c) with the appropriate arguments, and then arranging for the return value to be passed back to the user process in %eax. Finally, you need to implement syscall() in kern/syscall.c. Make sure syscall() returns -E_INVAL if the system call number is invalid. You should read and understand lib/syscall.c (especially the inline assembly routine) in order to confirm your understanding of the system call interface. Handle all the systems calls listed in inc/syscall.h by invoking the corresponding kernel function for each call.
>
> Run the user/hello program under your kernel (make run-hello). It should print "hello, world" on the console and then cause a page fault in user mode. If this does not happen, it probably means your system call handler isn't quite right. You should also now be able to get make grade to succeed on the testbss test.

首先要在 `trapentry.S` 添加 System Call 的入口，然后在 `trap_init` 中添加系统调用，最后在 `trap_dispatch` 中将 `T_SYSCALL` 转发到 `syscall` 即可，需要注意参数的传递和返回值的赋值。

接着就是在 `syscall.c` 中将 `SYS_cputs` 实现，那么在 switch 语句中直接添加一个 case 就可以了。

### User-mode startup

> **Exercise 8.** Add the required code to the user library, then boot your kernel. You should see user/hello print "hello, world" and then print "i am environment 00001000". user/hello then attempts to "exit" by calling sys_env_destroy() (see lib/libmain.c and lib/exit.c). Since the kernel currently only supports one user environment, it should report that it has destroyed the only environment and then drop into the kernel monitor. You should be able to get make grade to succeed on the hello test.

需要添加以下系统调用

1. `sys_getenvid`，用于在 `lib/libmain.c` 里获取当前的 envid。需要注意的是，envid 只有后10位用作 index，因此需要用 `ENVX` 宏取出。
2. `sys_env_destroy`，用于 `lib/exit.c` 的退出函数。

这两个系统调用只需要修改 `syscall` 函数就可以了。完成后 hello 程序就可以通过测试了。

### Page faults and memory protection

1. 缺页不仅会发生在用户程序中，而且会发生在内核的系统调用中。
2. 系统调用必须要检查传入的指针参数，否则可能会出现安全隐患。
3. 系统调用传入的指针参数可能会指向一个不存在的页表，因此发生缺页

JOS 的处理方法是

1. 在系统调用时，检查指针是否指向用户的内存空间。
2. 在系统调用之前，保证传入的指针都是指向存在的页表，因此内核不会发生缺页中断。

> **Exercise 9.** Change kern/trap.c to panic if a page fault happens in kernel mode.
>
> Hint: to determine whether a fault happened in user mode or in kernel mode, check the low bits of the tf_cs.
>
> Read user_mem_assert in kern/pmap.c and implement user_mem_check in that same file.
>
> Change kern/syscall.c to sanity check arguments to system calls.
>
> Boot your kernel, running user/buggyhello. The environment should be destroyed, and the kernel should not panic. You should see:
>
>		[00001000] user_mem_check assertion failure for va 00000001
>		[00001000] free env 00001000
>		Destroyed the only environment - nothing more to do!
>	
> Finally, change debuginfo_eip in kern/kdebug.c to call user_mem_check on usd, stabs, and stabstr. If you now run user/breakpoint, you should be able to run backtrace from the kernel monitor and see the backtrace traverse into lib/libmain.c before the kernel panics with a page fault. What causes this page fault? You don't need to fix it, but you should understand why it happens.

如何在 `page_fault_handler` 里判断当前的 trap 是否来自内核？可以通过 `tf->tf_cs & 3` 是否为0判断。

我们实现的3个系统调用中，唯一用到指针的就是 `sys_cputs`，因此在 `sys_cputs` 中加上 `user_mem_assert`。

在 `user_mem_check` 的实现中，我们有很多种方法，最简单的是使用 `pgdir_walk` 找出 pte 判断权限。由于 ULIM 之上的内存空间已经被 map 过了，权限判断自然会失败。因此我们只需要判断权限即可，不用判断是否超过 ULIM。

至于为什么 backtrace 会出错，原因是 backtrace 利用 call/ret 时的压栈惯例，而我们在 `env_run` 中运行用户程序时并没有使用 call，并且切换之后 esp 的值全变了，因此根据这种惯例就会访问到 USTACKTOP 以上的空内存，引发缺页中断。

> **Exercise 10.** Boot your kernel, running user/evilhello. The environment should be destroyed, and the kernel should not panic. You should see:
>
>		[00000000] new env 00001000
>		[00001000] user_mem_check assertion failure for va f010000c
>		[00001000] free env 00001000
		
如果上面的代码没有写错，那么这里也应该不用修改就能用。
