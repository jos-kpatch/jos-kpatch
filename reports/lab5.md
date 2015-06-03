title: Lab 4
author: Leedy

# Lab 5

## File system preliminaries

由于 JOS 是单用户操作系统，因此我们需要实现的文件系统不需要权限控制。为了简便起见，我们的文件系统也不支持硬链接、软链接、时间戳等。因此这个文件系统就不用提供 inodes 了：因为完全是一棵树的结构，我们将子文件夹与文件的信息直接存放在父文件夹的 data block 中。

## The File System

这部分 lab 要实现文件系统以下几个功能

1. Reading blocks into the block cache and flushing them back to disk
2. Allocating disk blocks
3. Mapping file offsets to disk blocks
4. Implementing read, write, and open in the IPC interface

### Disk Access

因为微内核的设计原则，我们将 IDE 驱动放在了用户态中。但我们仍然需要在内核中提供一些功能使得用户程序可以访问 IDE 硬件。最简单的方法就是 PIO。

PIO 由 EFLAGS 中的 IOPL 几个 bits 控制。由于我们不想让所有进程都可以访问 IO，因此需要将文件系统进程与其他进程区别对待。幸好的是，我们已经在 lab3 里完成了这个功能：Env 上有一个叫 type 的字段，用来区分进程种类。

> Exercise 1. i386_init identifies the file system environment by passing the type ENV_TYPE_FS to your environment creation function, env_create. Modify env_create in env.c, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.
>
> Make sure you can start the file environment without causing a General Protection fault. You should pass the "fs i/o" test in make grade.

直接给 FL_IOPL_3 的权限。

> **Question 1.** Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

不需要，EFLAGS 是在 Env.env_tf 中保存的，每次切换进程时都会切换 EFLAGS。

### The Block Cache

JOS 使用了一种神奇的磁盘访问+缓存技术：整个硬盘都映射到内存中。如果 cache 中已经有了，那么就直接读取内存的数据；如果 cache 中没有就会出现 page fault，然后自定义的 handler 就会去硬盘中取出需要的数据。

> **Exercise 2.** Implement the bc_pgfault and flush_block functions in fs/bc.c. bc_pgfault is a page fault handler, just like the one your wrote in the previous lab for copy-on-write fork, except that its job is to load pages in from the disk in response to a page fault. When writing this, keep in mind that (1) addr may not be aligned to a block boundary and (2) ide_read operates in sectors, not blocks.
>
> The flush_block function should write a block out to disk if necessary. flush_block shouldn't do anything if the block isn't even in the block cache (that is, the page isn't mapped) or if it's not dirty. We will use the VM hardware to keep track of whether a disk block has been modified since it was last read from or written to disk. To see whether a block needs writing, we can just look to see if the PTE_D "dirty" bit is set in the uvpt entry. (The PTE_D bit is set by the processor in response to a write to that page; see 5.2.4.3 in chapter 5 of the 386 reference manual.) After writing the block to disk, flush_block should clear the PTE_D bit using sys_page_map.
>
> Use make grade to test your code. Your code should pass "check_bc", "check_super", and "check_bitmap".

`bc_pgfault` 需要调用 `sys_page_alloc` 和 `ide_read` 两个函数。注意 `ide_read` 第三个参数是扇区的数量 BLKSECTS。`flush_block`

`flush_block` 中需要调用 `va_is_mapped`，`va_is_dirty`，`ide_write` 和 `sys_page_map`，最后一个可以参考上面 `bc_pgfault` 里的代码。

### The Block Bitmap

> **Exercise 3.** Use free_block as a model to implement alloc_block, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with flush_block, to help file system consistency.
>
> Use make grade to test your code. Your code should now pass "alloc_block".

Bitmap 是用来判断某个 block 是否被占用的。由于0、1、2三个 block 一定被占用，因此我直接从3开始判断。

### File Operations

> **Exercise 4.** Implement file_block_walk and file_get_block. file_block_walk maps from a block offset within a file to the pointer for that block in the struct File or the indirect block, very much like what pgdir_walk did for page tables. file_get_block goes one step further and maps to the actual disk block, allocating a new one if necessary.
>
> Use make grade to test your code. Your code should pass "file_open", "file_get_block", and "file_flush/file_truncated/file rewrite", and "testfile".

这个是比较难的，和我们以前写过的 `pgdir_walk` 比较类似，`block_walk` 是两段寻址，因此要先看 `filebno` 的大小。这里 `ppdiskbno` 需要通过传地址修改指针的方式返回，非常容易搞错。

需要注意的是，`file_block_walk` 中 `**ppdiskbno` 可能为不存在（注释里写成了 `*ppdiskbno`），为了保证不出错，第一是 `file_block_walk` 中需要将新分配的 `f->f_indirect` 的 block 清零，第二是 `file_get_block` 中需要检查，如果不存在还需要再分配一个 block。

### The file system interface

由于我们在用户态实现了文件系统，所以对于其他用户进程而言，IO 操作全部变成了 IPC 操作。

> **Exercise 5.** Implement serve_read in fs/serv.c.
>
> serve_read's heavy lifting will be done by the already-implemented file_read in fs/fs.c (which, in turn, is just a bunch of calls to file_get_block). serve_read just has to provide the RPC interface for file reading. Look at the comments and code in serve_set_size to get a general idea of how the server functions should be structured.
>
> Use make grade to test your code. Your code should pass "serve_open/file_stat/file_close" and "file_read" for a score of 65/145.

fs/fs.c 与 fs/serv.c 里有巨量的代码，然而并不要求我们写……我们只需要完成 `serve_read` 函数。这个函数只是调用 `openfile_lookup` 和 `file_read` 的 wrapper 而已，很简单。

> **Exercise 6.** Implement serve_write in fs/serv.c and devfile_write in lib/file.c.
>
> Use make grade to test your code. Your code should pass "file_write", "file_read after file_write", "open", and "large file" for a score of 85/145.

`serve_write` 和 `serve_read` 基本相同，只需将 `file_read` 换成 `file_write` 即可。

`devfile_write` 是库代码，仿照上面的 `devfile_read` 很容易就写出来了。

## Spawning Processes


