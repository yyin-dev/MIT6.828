# Chapter 1 Operating sytem organization

Three requirements for an OS: multiplexing, isolation, interaction (between processes). Xv6 uses monolithic kernel.The chapter traces how the first process is created. 

## Abstracting physical resources

OS as a library for managing hardware, i.e. cooperative time-sharing, doesn't provide enough isolation. Unix system call interface abstracts hardware as services and provides stronger isolation. 

## User mode, kernel mode and system calls

Hardware support for isolation: user mode and kernel mode. Only the kernel mode can execute priviledged instuctions. If a process in user mode tries to execute a priviledged instruction, OS kills the process. An application running in user mode is said to be running in *user space*, while software running in kernel mode is said to be running in *kernel space*. The software running in kernel space is called the *kernel*. 

An application that wants to read/write a file (a priviledged instruction) must switch to the kernel mode to do so. The processor provides a special "trap" instruction to **switch from user mode to kernel mode**, and **enters the kernel at an entry point specified by the kernel**. Then the kernel validates system call arguments, check the access level of the application, and deny/execute the instruction. 

## Kernel organization

Monolithic kernel: the entire OS runs in the kernel mode, i.e., all system call runs in kernel mode. Pro: All parts of OS are given full hardware priviledge and it's easy for different parts to cooperate. Con: The interface between different parts can be complex and hard to debug. Not enough isolation.

Microkernel: minimize the amount of OS code that runs in kernel mode, and execute some OS code in user mode. OS services, like shell, file system, etc, run as processes in the user space called servers. Applications interact with servers using inter-process communication (IPC).

Xv6 is implemented as a monolithic kernel. 

## Process overview

Isolation should be enforced for processes. A process is a running program. The process provides to the program:

- An illusion that it has its own CPU;
- A seemingly private memory address space.

Xv6 maintains a separate page table for each process. 

Each process's virtual address space contains (1) kernel's instruction and data (BIOS, kernel text, data, etc), and (2) user program's memory (user text, data, stack, heap, etc). To leave enough space for user memory, xv6 maps kernel at high address, starting at `0x80100000`. The 1MB from `0x80000000` to `0x80100000` is for BIOS. So `[0x00000000, 0x8000000]` is user program's memory, `[0x80000000, 0x80100000]` is BIOS's memory, `[0x80100000, 0xFFFFFFFF]` is kernel's memory.

```
Virtual address space:

+-----------------------+ <- 0xFFFFFFFF
|						|
|	kernel text & data 	|
|						|
+-----------------------+ <- 0x80100000
|		BIOS			|						kernel
+-----------------------+ <- 0x80000000 	------------
|						|						user
|  						|
|	user program memory	|
|						|
|						|
+-----------------------+ <- 0x00000000
```

The xv6 kernel maintains state of a process into a `struct proc`. The most important **state of a process** is (1) the page table, (2) kernel stack, and (3) run state. We use `p->xxx` to denote the elements of a `struct proc`.

Each process has a thread (of execution). To switch transparently between threads, the kernel suspends the current thread and resumes another thread. Most of the **states of a thread** (local variables, function call return address) is stored on the thread's stack. Each process has two stacks: a *user stack* and a *kernel stack*. When the process is executing user instruction, the user stack is used. When the process enters the kernel, the kernel code uses the kernel stack. The kernel stack is isolated from the user code, so that the kernel code can execute even if the user stack is corrupted.

**Xv6 system call mechanism**: When a process makes a system call, the processor switches to kernel stack, switches to kernel mode, and executes the kernel code that implement the system call. When the system call completes, the kernel switches to user mode, switches back to user stack, and resumes executing user instruction after the system call instruction. A thread can block in kernel for I/O.

`p->state` indicates if the process is allocated, ready to run, running, blocking, or exiting. `p->pgdir` holds the process's page table.

Now, we examine how the kernel creates the first address space for itself, how it creates and starts the first process, and how that process performs the first system call. 

## Code: the first address space

This is very similar to Lab1.

When a PC powers on, it executes BIOS, when loads boot loader from disk and execute it. The boot loader loads xv6 kernel code into memory and executes from `entry`. The x86 paging hardware is not enabled when the kernel starts; virtual addresses map directly to physical memory address.

 ```
bootasm.S -> bootmain.c -> entry.S -> main.c
<---- boot loader ---->	  <---- kernel ---->
 ```

The boot loader loads the xv6 kernel at physical address `0x100000`. The kernel is not loaded at `0x0` as the address range `0xa0000:0x1000000` contains I/O devices (the hole). 

![img](https://pic3.zhimg.com/80/v2-dbd4f3f3e67686bc126d873f1b354ad2_720w.jpg)

The picture is not exactly correct about `[0x00000000, 0x00100000]` in the physical address space. Recall Lab1, `[0x00000000, 0x000a0000]` is "low memory", `[0xa0000, 0x100000]` are VGA, I/O devices, and BIOS. 

The boot loader loads the xv6 kernel at physical address `0x100000`, but the kernel expects it to find its instruction and data at `0x80100000`. A page table is set up to map virtual address starting at `0x80000000` to physical address starting at `0x0000000`, and virtual memory address `0x00000000` still maps to  physical address starting at `0x0000000`.

The page table is defined in `main.c`. Entry 0 maps virtual address `[0x0, 0x400000]` to physical address `[0x0, 0x400000]`. Entry 512 maps virtual addresses `[KERNBASE, KERNBASE+0x400000]` to physical addresses `[0x0, 0x400000]`. This restricts the kernel instruction and data to 4MB. Keep in mind that, **up to now, virtual address maps directly to physical address** and **kernel executes at low address**.

In `entry.S`'s `entry`, it loads the physical address of `entrypgdir` into %cr3, which expects a physical address. The symbol `entrypgdir` refers to the address in high memory, and `V2P_WO` computes the physical address. Finally, enable paging hardware by setting %cr0. After paging is enabled, the processor still executes at low addresses. This's ok since `entrypgdir` maps low addresses. If xv6 omitted entry 0, the computer would crash. 

Now `entry` needs to transfer to the kernel's C code, and **starts executing at high address**. First it makes %esp points to high address. Then it jumps to `main`, which is also a high address, using `jmp *%eax`. The indirect jump is needed to run at high addresses. Now the kernel runs at high address in function `main`. We can remove entry 0 in `entrypgdir` now.

Note: in fact, it seems that using direct jump `jmp main` would give the same assembly (as below) and the same behavior in qemu? I doubt the indirect jump is needed.

```
(gdb) x/20i 0x10000c
   0x10000c:	mov    %cr4,%eax
   0x10000f:	or     $0x10,%eax
   0x100012:	mov    %eax,%cr4
   0x100015:	mov    $0x109000,%eax
   0x10001a:	mov    %eax,%cr3
   0x10001d:	mov    %cr0,%eax
   0x100020:	or     $0x80010000,%eax
   0x100025:	mov    %eax,%cr0
   0x100028:	mov    $0x8010b5c0,%esp
   0x10002d:	mov    $0x80102ea0,%eax 	// high address
   0x100032:	jmp    *%eax
```

Anyway, this detail is not important.



## Code: creating the first process

`main` creates the first process by calling `userinit`. `Userinit` first calls `allocproc`, which allocates a slot in the process table, and initialize the parts of the process state required for its **kernel thread** to run. `Allocproc` is called for each process, while `userinit` is called only for the first process. 

`Allocproc` does the following: 

- Try to allocate a slot (a `struct proc`) in the process table;

- Try to allocate a kernel stack for the proess's kernel thread;

- Set up the kernel stack. `Allocproc` is designed to be used by `fork` as well as when creating the first process. `Allocproc` sets up the stack such that the process "returns" to user space when it first runs. 

  Basically, it sets up program counter values such that the process's kernel thread would execute `forkret` and then `trapret`, by setting `p->context->eip` to `forkret`, and putting a pointer to `trapret` use above `p->context`. `Trapret` restores user registers from values on top of the stack and jumps into the process. This setup is the same for ordinary `fork` and the creating the first process. 

  ```
  +---------------+ <--- Kernel stack grows from here
  |				|
  |				|
  |				|
  +---------------+ <--- p->tf
  | 	trapret		|
  +---------------+ <--- Address forkret would return to
  |				|
  |				|
  +---------------+ <--- p->context
  |				|
  |	Empty		|
  +---------------+ <--- p->kstack
  ```

  Essentially, `allocproc` sets up the kernel stack such that it looks like it trapped into kernel code. 

Then `setupkvm` is called to create an address space for the process. `Inituvm` allocates one page of physical memory, maps virtual address `0x0` to that memory, adn copies the binary of `initcode.S` to that page. Then `userinit` sets up the trapframe.



## Code: Running the first process

```
mpmain()
	scheduler() {
		finds RUNNABLE process;
		switchuvm();
		swtch(&(c->scheduler), p->context);
		...
	}
```

