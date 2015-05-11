title: Lab 4
author: Leedy

# Lab 4

## Part A: Multiprocessor Support and Cooperative Multitasking

这一部分我们需要实现多处理器支持与合作式的多任务。

### Multiprocessor Support

SMP：对称多处理器，即所有的处理器对系统资源都有相同的访问权限。但在启动过程中，还是只有一个发挥作用。只有当 BSP 调用 `boot_aps` 之后，其他处理器才开始工作。

在 SMP 系统中，每个 CPU 都有一个中断控制器（LAPIC）。在 Lab 3 中，我们只接触到内部中断，因此不需要用到 APIC。在 Lab 4 中，由于需要多处理器之间互相联络，以及 part C 中配置时钟终端，因此我们需要使用 APIC。

LAPIC 是通过 MMIO 来访问的，在 `lapic_init` 中先要调用 `mmio_map_region` 初始化 MMIO 用的内存。

> **Exercise 1.** Implement mmio_map_region in kern/pmap.c. To see how this is used, look at the beginning of lapic_init in kern/lapic.c. You'll have to do the next exercise, too, before the tests for mmio_map_region will run.

这个只需要调用 `boot_map_region` 即可，权限需要设置为 `PTE_W | PTE_PCD | PTE_PWT`。

#### Application Processor Bootstrap

首先需要由 BSP 检测有几个 CPU，这个是由 mpconfig.c 中的一堆代码完成，本质上是去读 BIOS 的内存区域，这里不需要写代码，因此我也没有逐行研究……

然后是初始化 BSP 的 LAPIC，这个也不需要我们写代码，打开看了两眼。

接下来，BSP 调用 `boot_aps` 来启动 AP 们。其中，`boot_aps` 的代码有点神奇，它将 `mpentry_start` 到 `mpentry_end` 的代码搬到 `MPENTRY_PADDR`。而 `MPENTRY_PADDR` 居然不是我以为 0x7C00。后来看到 mpentry.S 里的注释得知，AP 启动时的 CS:IP 可以由启动指令动态的设置，因此在 `lapic_startap` 中有一个参数正是 `MPENTRY_PADDR`。在 `boot_aps` 的最后，BSP 等待其他的处理器将 `cpu_status` 设置为 `CPU_STARTED`。

> **Exercise 2.** Read boot_aps() and mp_main() in kern/init.c, and the assembly code in kern/mpentry.S. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of page_init() in kern/pmap.c to avoid adding the page at MPENTRY_PADDR to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated check_page_free_list() test (but might fail the updated check_kern_pgdir() test, which we will fix soon).

在 `page_init` 中将 `pages[PGNUM(MPENTRY_PADDR)]` 标记为已使用即可。然后在运行 QEMU 的时候发现了自己在 `mmio_map_region` 里的一个 bug：没有考虑到 size 对齐的情况。

> **Question 1.** Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S? 
>
> Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

使用宏 `MPBOOTPHYS` 的目的是让符号的地址与装载地址相同。而 `boot.S` 里不需要的原因是我们在链接时已经为 boot 模块的编译地址与装载地址做了特殊的处理，使得它们都指向 0x7C00。

#### Per-CPU State and Initialization

在 mpentry.S 执行结束后，AP 们会跳转到 `mp_main`，执行其他的初始化操作，包括

1. 每个 CPU 内核栈的初始化。这个在 memlayout 里可以看到，KSTACKTOP 下面分为多个不同的内核栈。
2. 每个 CPU TSS 的初始化。
3. 每个 CPU 的当前运行进程的初始化。
4. 每个 CPU 一些配置寄存器的初始化（cr3，gdt，idt 等）。

> **Exercise 3.** Modify mem_init_mp() (in kern/pmap.c) to map per-CPU stacks starting at KSTACKTOP, as shown in inc/memlayout.h. The size of each stack is KSTKSIZE bytes plus KSTKGAP bytes of unmapped guard pages. Your code should pass the new check in check_kern_pgdir().

这里调用 NCPU 次 `boot_map_region` 即可。

> **Exercise 4.** The code in trap_init_percpu() (kern/trap.c) initializes the TSS and TSS descriptor for the BSP. It worked in Lab 3, but is incorrect when running on other CPUs. Change the code so that it can work on all CPUs. (Note: your new code should not use the global ts variable any more.)

我先将 global 的 ts 给删掉了，然后将所有的 ts 都替换为 `thiscpu->cpu_ts`，将 `GD_TSS0 >> 3` 替换为 `(GD_TSS0 >> 3) + cpunum()`，最后还需要将 `ltr` 的参数换成 `GD_TSS0 + (cpunum() << 3)`。

运行之后出现了预测中的结果，但同时也进入重启死循环……

#### Locking

加锁是很有必要的，因为内核态各个资源（页表、CPU 信息）都是共享的。现在我们暂时使用大内核锁，只允许一个 CPU 进入内核态。

> **Exercise 5.** Apply the big kernel lock as described above, by calling lock_kernel() and unlock_kernel() at the proper locations.

有以下几个地方需要加锁。

1. In i386_init(), acquire the lock before the BSP wakes up the other CPUs.
2. In mp_main(), acquire the lock after initializing the AP, and then call sched_yield() to start running environments on this AP.
3. In trap(), acquire the lock when trapped from user mode. To determine whether a trap happened in user mode or in kernel mode, check the low bits of the tf_cs.
4. In env_run(), release the lock right before switching to user mode. Do not do that too early or too late, otherwise you will experience races or deadlocks.

> **Question 2.** It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

因为我们加锁与解锁分别在中断发生之后与中断返回之前。如果一个 CPU 在处理中断，另一个 CPU 也发生了中断，虽然另一个 CPU 卡在了 trap 开头，但已经将错误的 trapframe 压到了栈里。因此当前 CPU `env_pop_tf` 时就可能会出错。

### Round-Robin Scheduling

这里一堆话只需要实现一个函数 `sched_yield`，在 sched.c 中。

> **Exercise 6.** Implement round-robin scheduling in sched_yield() as described above. Don't forget to modify syscall() to dispatch sys_yield().
>
> Make sure to invoke sched_yield() in mp_main.
>
> Modify kern/init.c to create three (or more!) environments that all run the program user/yield.c.

首先，将 `sched_yield` 实现了，需要注意的是 curenv 可能为空，因此需要判断一下。然后在 syscall 里将 `sys_yield` 给添加上（三行） 就 OK 了。

接下来去 init.c 里将 `	ENV_CREATE(user_yield, ENV_TYPE_USER);` 复制三遍，运行！

很不幸的是，当 CPUS 指定为2时无法正常运行，卡在了 `Hello, I am environment 00001000.` 之前，猜测是发生了死锁。经检查发现 `mp_main` 中忘了调用 `sched_yield` 因此某个 AP 将整个内核给卡住了……

> **Question 3.** In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

`env_run` 中 `lcr3` 之后使用的变量是 `curenv`，而 `curenv` 存放在 KERNBASE 之上，因此在前后两个页表里都是一样的映射。

> **Question 4.** Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

原因就是要保证一个进程执行的连续性。发生的位置是在 `trap` 函数中，将内核栈中的 trapframe 赋值给 `curenv->env_tf`。

### System Calls for Environment Creation

由于 JOS 微内核的设计，我们将 fork 系统调用拆分成多个子系统调用分别实现，使得 fork 的主要逻辑在用户态实现。这些系统调用有

1. sys_exofork，这个系统调用是 fork 的第一步，创建一个新进程（其实就是 `env_alloc` 的 wrapper），但什么都不做。为新进程返回0，为旧进程返回新进程的 `env_id`。
2. sys_env_set_status，这个很简单，设置某个进程的运行状态。
3. sys_page_alloc，这个是 `page_alloc` 与 `page_alloc` 的 wrapper。
4. sys_page_map，这个系统调用比较神奇，将一个进程的某个页 map 到另一个进程的某个地址。
5. sys_page_unmap，和上一个相反。

> **Exercise 7.** Implement the system calls described above in kern/syscall.c. You will need to use various functions in kern/pmap.c and kern/env.c, particularly envid2env(). For now, whenever you call envid2env(), pass 1 in the checkperm parameter. Be sure you check for any invalid system call arguments, returning -E_INVAL in that case. Test your JOS kernel with user/dumbfork and make sure it works before proceeding.

需要注意的是，所有的系统调用都需要仔细检查参数，参数越多需要检查的也就越多。最典型的就是 `sys_page_alloc` 的 perm 参数，我使用的方法如下

	if ((perm & PTE_U) == 0 || (perm & PTE_P) == 0 || (perm & ~PTE_SYSCALL) != 0)
		return -E_INVAL;

## Part B: Copy-on-Write Fork

由于大多数情况下我们 fork 之后就会直接调用 exec，因此 fork 时的内存拷贝就变得没有太大意义了，而这恰好是 fork 时最浪费时间的地方，因此需要提供 COW 的 fork。COW 的 fork 细节大家都懂。具体实现起来，就是在 page fault 的时候复制内存页而不是 fork 的时候复制。

又，因为 JOS 的微内核设计，fork 是在用户态实现的，因此 COW 也需要在用户态实现，因此 page fault handler 也需要在用户态实现（怎么感觉比宏内核还复杂）。因此第一步我们需要提供一个设置 page fault handler 的系统调用。

### User-level page fault handling

COW 不仅仅是 user-level page fault handling 的唯一用途，它还有其他用途，例如实现 text 段的动态加载、stack 与 BSS 的不同分配规则等。

#### Setting the Page Fault Handler

> **Exercise 8.** Implement the sys_env_set_pgfault_upcall system call. Be sure to enable permission checking when looking up the environment ID of the target environment, since this is a "dangerous" system call.

这个和上面 `sys_env_set_status` 差不多，先检查参数，然后赋值。

#### Normal and Exception Stacks in User Environments

因为 page fault 很可能出现在栈空间不够的情况下，这时候如果 exception stack 仍然使用原先的 esp 的话就会粗大事，所以我们需要单独使用一个 exception stack。由于 exception 不会执行太多代码，所以我们只提供一个内存页就可以了。

#### Invoking the User Page Fault Handler

现在，当 page fault 发生时，如果已经注册了 page fault handler，那么内核先将一个 `UTrapframe` 的结构 push 到 exception stack 上，然后切换到用户态执行 handler 的代码。

有一种情况是，page fault 是在另一个 page fault handler 运行时触发的，这时需要将 trapframe 结构放到  tf_esp 下面，而不是 UXSTACKTOP。

> **Exercise 9.** Implement the code in page_fault_handler in kern/trap.c required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?)

这里要理清楚 tf、utf 两者之间各个寄存器的关系与内存的布局，比较好的方法是先将 utf 的内存地址弄清楚，然后通过指针与结构体赋值。最后需要将 tf 里的 eip 和 esp 改掉，让 `env_run` 的时候可以顺利地跳到 `env_pgfault_upcall` 上。

#### User-mode Page Fault Entrypoint

> **Exercise 10.** Implement the `_pgfault_upcall` routine in lib/pfentry.S. The interesting part is returning to the original point in the user code that caused the page fault. You'll return directly there, without going back through the kernel. The hard part is simultaneously switching stacks and re-loading the EIP.

调用关系是 `_pgfault_upcall` --> `_pgfault_handler`，`_pgfault_upcall` 的作用是

1. 准备 `_pgfault_handler` 的参数。
2. `_pgfault_handler` 返回后跳转到原来的 eip 继续执行（不需要经过内核）。

这里有个矛盾之处是，如何保证能回到原来的状态。我们在 `env_run` 里用的是 iret，而 iret 是特权指令（因为能切 CPL），所以这里没法用。由于 jmp 必须要依赖于一个寄存器，ret 不能切 esp 的值，我们最终的解决方法是先将 eip push 到异常发生时的栈上，然后先将 esp 切过去，再 ret。

这里有一些次序必须要搞清楚，比如先恢复通用寄存器，再恢复 eflags，再恢复 esp，再恢复 eip。

恢复通用寄存器时，可以直接调用 pushal，因为 pushal 并其实不会恢复 esp 的值。

> **Exercise 11.** Finish set_pgfault_handler() in lib/pgfault.c.

这个 lab 怎么那么多 exercises……

干两件事：分配 UXSTACKTOP 的内存页；设置真·缺页中断 handler。记得在系统调用时第一个参数填0指自己。

#### Testing

> Make sure you understand why user/faultalloc and user/faultallocbad behave differently.

原因是 faultalloc 是在 user space dereferenced 的，所以直接缺页中断，而 sys_cputs 是在 kernel dereferenced 的，因此被认为是恶意代码。

### Implementing Copy-on-Write Fork

下面需要实现3个函数

1. fork。基本流程是：`set_pgfault_handler`；`sys_exofork`；将共享的可写的页面标记为 COW（`duppage`，两边都要）；设置子进程的 `thisenv`（本来应该在 libmain 里设置）和 `pgfault_handler`（因为我们 exofork 的时候没有复制这一项）；`sys_env_set_status`；
2. duppage。将一个页 map 到目标进程的相同位置，如果这个页可写，则标记为 COW。
3. pgfault。先检查是否是 write page fault（`FEC_WR`）和 POW，然后复制发生缺页的页，remap。

> **Exercise 12.** Implement fork, duppage and pgfault in lib/fork.c.

写代码的时候发现：还是很难的。

fork 里需要注意

1. entry.S 和 memlayout.h 里定义了 'envs', 'pages', 'uvpt', 与 'uvpd'，可以作为数组使用。顺便复习了一下 mmu.h 里的内容……
2. 扫描要复制的页时最多只需要到 USTACKTOP，exception stack 不需要。
3. 不能在子进程中调用 `set_pgfault_handler`，因为子进程所有的栈都是 COW 的，根本没法自己调用函数，因此需要在父进程中设置好。
4. 写代码时会发现：`_pgfault_upcall` 的符号并不是 global 的（坑爹啊）。可以自己加 `extern void _pgfault_upcall(void)`，也可以用 `thisenv->env_pgfault_upcall`。

duppage 里有一个问题

> Exercise: Why might we need to mark ours copy-on-write again if it was already copy-on-write at the beginning of this function?

这个问题一开始想了半天，然后去网上查，发现很简单：duppage 中可能会引发缺页，于是可能在判断时这个页还是 COW，但第一次调用 `sys_page_map` 后这个页就变成可写的了，因此以防万一：我们总是再 map 一次。

pgfault 就非常简单了，提示里连几个系统调用都告诉你了。

最后运行！发现一个小 bug：fork 里在扫描复制的页时上届搞错了，`i < PDX(USTACKTOP)`，应该是 `i <= PDX(USTACKTOP)` 或者 `i < PDX(ROUNDUP(USTACKTOP, PTSIZE)`。

## Part C: Preemptive Multitasking and Inter-Process communication (IPC)

终于到达最后一个 part 了！

### Clock Interrupts and Preemption

实现时钟中断（熟悉的 8259A）和抢占式任务调度。

#### Interrupt discipline

外部中断又叫 IRQ 中断，中断号在 `IRQ_OFFSET` 和 `IRQ_OFFSET + 15` 之间。

`trapentry.S` 这个文件我已经反复修改过很多次了，但总是感觉不太满意……

> **Exercise 13.** Modify kern/trapentry.S and kern/trap.c to initialize the appropriate entries in the IDT and provide handlers for IRQs 0 through 15. Then modify the code in env_alloc() in kern/env.c to ensure that user environments are always run with interrupts enabled.
>
> The processor never pushes an error code or checks the Descriptor Privilege Level (DPL) of the IDT entry when invoking a hardware interrupt handler. You might want to re-read section 9.2 of the 80386 Reference Manual, or section 5.8 of the IA-32 Intel Architecture Software Developer's Manual, Volume 3, at this time.
>
> After doing this exercise, if you run your kernel with any test program that runs for a non-trivial length of time (e.g., spin), you should see the kernel print trap frames for hardware interrupts. While interrupts are now enabled in the processor, JOS isn't yet handling them, so you should see it misattribute each interrupt to the currently running user environment and destroy it. Eventually it should run out of environments to destroy and drop into the monitor.

1. 在 `trapentry.S` 里添加 IRQ 的条目。
2. 在 `trap_init` 里将 IRQ 条目添加到 IDT 里。
3. 在 `env_alloc` 里将用户进程默认的 interrupt flag 打开。复习一下操作 interrupt flag 的三种方式：sti, cli, popf，它们都是特权指令。

#### Handling Clock Interrupts

> **Exercise 14.** Modify the kernel's trap_dispatch() function so that it calls sched_yield() to find and run a different environment whenever a clock interrupt takes place.
>
> You should now be able to get the user/spin test to work: the parent environment should fork off the child, sys_yield() to it a couple times but in each case regain control of the CPU after one time slice, and finally kill the child environment and terminate gracefully.

这里就比较简单，只需要写四行就可以了。

### Inter-Process communication (IPC)

#### IPC 

IPC 在 JOS 里是用两个系统调用实现的：`sys_ipc_try_send` 和 `sys_ipc_recv`。

#### Sending and Receiving Messages

1. `sys_ipc_recv`：将自己标记为 `ENV_NOT_RUNNABLE`，等待其他进程发送消息。可以传一个参数用于接收更多的信息。
2. `sys_ipc_try_send`：发送消息给任意一个进程，要求目标进程必须在等待状态，发送消息后将目标进程重新标记为 `ENV_RUNNABLE`。

> **Exercise 15.** Implement sys_ipc_recv and sys_ipc_try_send in kern/syscall.c. Read the comments on both before implementing them, since they have to work together. When you call envid2env in these routines, you should set the checkperm flag to 0, meaning that any environment is allowed to send IPC messages to any other environment, and the kernel does no special permission checking other than verifying that the target envid is valid.
>
> Then implement the ipc_recv and ipc_send functions in lib/ipc.c.
>
> Use the user/pingpong and user/primes functions to test your IPC mechanism. user/primes will generate for each prime number a new environment until JOS runs out of environments. You might find it interesting to read user/primes.c to see all the forking and IPC going on behind the scenes.

分为两部分，一部分是 kernel 的系统调用实现，另一部分是用户态 `ipc_recv` 和 `ipc_send` 的实现。

前半部分最大的问题是参数检查，其中 `env_ipc_perm` 的设置尤为复杂：如果出现任何错误，则不变，如果成功 map 则设置为 perm，否则设置为0。

后半部分比较容易。

运行一下 primes，跑了整整一分钟，第1022个素数是8147！

于是终于做完了整个 lab。