# Lab4 PartA - Multiprocessor support and cooperative multitasking

## Multiprocessor support

Explains SMP model, BSP, AP, LAPIC, MMIO.  

SMP: Symmetric Multiprocessing. All CPUs have equivalent access to system resources such as memory and I/O buses.

BSP: Bootstrap Processor. 

AP: Application Processor. Activated by BSP after the OS is up and running.

LAPIC: Local APIC unit. For delivering interrupts to CPUs.

MMIO: Memory-mapped I/O.

> **Exercise 1.** Implement `mmio_map_region` in `kern/pmap.c`. To see how this is used, look at the beginning of `lapic_init` in `kern/lapic.c`. You'll have to do the next exercise, too, before the tests for `mmio_map_region` will run.

## Application Processor Bootstrap

Explains `mp_init`, `boot_aps`.

> **Exercise 2.** Read `boot_aps()` and `mp_main()` in `kern/init.c`, and the assembly code in `kern/mpentry.S`. Make sure you understand the control flow transfer during the bootstrap of APs. Then modify your implementation of `page_init()` in `kern/pmap.c` to avoid adding the page at `MPENTRY_PADDR` to the free list, so that we can safely copy and run AP bootstrap code at that physical address. Your code should pass the updated `check_page_free_list()` test (but might fail the updated `check_kern_pgdir()` test, which we will fix soon).

Control flow transfer during the bootstrap of APs:

- `i386_init` calls `mp_init` and `lapic_init`. `mp_init` reads information from BIOS about the CPUs.
- `i386_init` calls `boot_aps`. `boot_aps` moves `mpentry.S` into memory and calls `lapic_startup` to start the processor running. 
- In `mpentry.S`, changes from real-mode to protected-mode and jumps to `mp_main`. `mp_main` sets up things and send `boot_aps` this CPU is up, so that `boot_aps` tries to start up the next CPU. 

```diff
$ git diff
diff --git a/kern/pmap.c b/kern/pmap.c
index 546663f..432e496 100644
--- a/kern/pmap.c
+++ b/kern/pmap.c
@@ -317,18 +317,20 @@ page_init(void)
        // free pages!
        size_t i;
        for (i = 0; i < npages; i++) {
                        // (1)
-               } else if (PGSIZE <= pageStartVA && pageStartVA < npages_basemem * PGSIZE) {
+               } else if (pageStartPA == ROUNDDOWN(MPENTRY_PADDR, PGSIZE)) {
+                       // Lab 4: mpentry.S code
+               } else if (PGSIZE <= pageStartPA && pageStartPA < npages_basemem * PGSIZE) {
                        // (2) Free
                        pages[i].pp_ref = 0;
                        pages[i].pp_link = page_free_list;
                        page_free_list = &pages[i];
@@ -632,7 +634,14 @@ mmio_map_region(physaddr_t pa, size_t size)
        // Hint: The staff solution uses boot_map_region.
        //
        // Your code here:
-       panic("mmio_map_region not implemented");
+       uint32_t sz = ROUNDUP(size, PGSIZE);
+       if (base + sz > MMIOLIM) panic("MMIOLIM overflowed!\n");
+
+       boot_map_region(kern_pgdir, base, sz, pa, PTE_PCD|PTE_PWT);
+       void *reserved = (void *)base;
+       base += sz;
+
+       return reserved;
 }
 
 static uintptr_t user_mem_check_addr;
```

This passes `check_page_free_list` but fails `check_kern_pgdir`.

> **Question**
>
> 1. Compare `kern/mpentry.S` side by side with `boot/boot.S`. Bearing in mind that `kern/mpentry.S` is compiled and linked to run above `KERNBASE` just like everything else in the kernel, what is the purpose of macro `MPBOOTPHYS`? Why is it necessary in `kern/mpentry.S` but not in `boot/boot.S`? In other words, what could go wrong if it were omitted in `kern/mpentry.S`?
>    Hint: recall the differences between the link address and the load address that we have discussed in Lab 1.

1. Compare `mpentry.S` with `boot.S`+`entry.S`:

   ```
   	myentry.S									boot.S
   Disable interrupts;                      Same action;
   Set up segment registers;                Same action;
                                            Enable A20 gate;
   Switch to protected-mode;                Same action;
   Set up procted-mode segment registers;   Same action;
   Set up initial page table;               Call bootmain(), which would jump to entry.S;	
   Jump to mp_main();								
                                                entry.S
                                            Load kern_pgdir as page tables;
                                            Jump to high address;
                                            Call i386_init();
   ```

   The purpose of macro `MPBOOTPHYS` is to convert virtual address to physical address.

   ```
   $ objdump -h obj/boot/boot.out
   
   obj/boot/boot.out:     file format elf32-i386
   
   Sections:
   Idx Name          Size      VMA       LMA       File off  Algn
     0 .text         0000019f  00007c00  00007c00  00000074  2**2
                     CONTENTS, ALLOC, LOAD, CODE
   ...
   $ objdump -h obj/kern/kernel
   
   obj/kern/kernel:     file format elf32-i386
   
   Sections:
   Idx Name          Size      VMA       LMA       File off  Algn
     0 .text         00005c69  f0100000  00100000  00001000  2**4
                     CONTENTS, ALLOC, LOAD, READONLY, CODE
   ...
   ```

   In `boot.S`, link address is identical to load address (`boot.S` is loaded at low address) and paging is not enabled (so virtual address equals to physical address), thus no need for conversion.
   On the other hand, **as part of the kernel code**, `mpentry.S` is linked at address above `KERNBASE`, but paging is not enabled yet in `mpentry.S` and we should get its physical address with macro `MPBOOTPHYS`.

   In short, `boot.S` is loaded and linked at low, while `mpentry.S` is linked at high but loaded at low.

## Per-CPU State and Initialization

Explains per-CPU state in JOS and changes from previous labs. Per-CPU state: kernel stack, TSS, `curenv`, register set.

> **Exercise 3.** Modify `mem_init_mp()` (in `kern/pmap.c`) to map per-CPU stacks starting at `KSTACKTOP`, as shown in `inc/memlayout.h`. The size of each stack is `KSTKSIZE` bytes plus `KSTKGAP` bytes of unmapped guard pages. Your code should pass the new check in `check_kern_pgdir()`.

```diff
$ git diff
diff --git a/kern/pmap.c b/kern/pmap.c
index 432e496..44c5a4d 100644
--- a/kern/pmap.c
+++ b/kern/pmap.c
@@ -277,6 +277,12 @@ mem_init_mp(void)
        //
        // LAB 4: Your code here:
 
+       for (int i = 0; i < NCPU; ++i) {
+               uint32_t region_sz = KSTKSIZE + KSTKGAP;
+               uint32_t base_va = KSTACKTOP - i * region_sz - KSTKSIZE;
+               uint32_t base_pa = PADDR(percpu_kstacks[i]);
+               boot_map_region(kern_pgdir, base_va, KSTKSIZE, base_pa, PTE_P|PTE_W);
+       }
 }
```

Output:

```
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 4 CPU(s)
kernel panic on CPU 0 at kern/trap.c:344: kernel page fault!
```

>  **Exercise 4.** The code in `trap_init_percpu()` (`kern/trap.c`) initializes the TSS and TSS descriptor for the BSP. It worked in Lab 3, but is incorrect when running on other CPUs. Change the code so that it can work on all CPUs. (Note: your new code should not use the global `ts` variable any more.)

```diff
$ git diff
diff --git a/kern/pmap.c b/kern/pmap.c
index 44c5a4d..1073f7a 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -158,20 +158,25 @@ trap_init_percpu(void)
        //
        // LAB 4: Your code here:
 
+       // Read the definition of `gdt` in `kern/env.c` before coding.
+
        // Setup a TSS so that we get the right stack
        // when we trap to the kernel.
-       ts.ts_esp0 = KSTACKTOP;
-       ts.ts_ss0 = GD_KD;
-       ts.ts_iomb = sizeof(struct Taskstate);
+       uint8_t id = thiscpu->cpu_id;
+
+       // Note that percpu_kstacks[i] refers to the limit of CPU i's kernel stack.
+       thiscpu->cpu_ts.ts_esp0 = (uintptr_t) (percpu_kstacks[id] + KSTKSIZE);
+       thiscpu->cpu_ts.ts_ss0 = GD_KD;
+       thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);
 
        // Initialize the TSS slot of the gdt.
-       gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t) (&ts),
+       gdt[(GD_TSS0 >> 3) + id] = SEG16(STS_T32A, (uint32_t) (&thiscpu->cpu_ts),
                                        sizeof(struct Taskstate) - 1, 0);
-       gdt[GD_TSS0 >> 3].sd_s = 0;
+       gdt[(GD_TSS0 >> 3) + id].sd_s = 0;
 
        // Load the TSS selector (like other segment selectors, the
        // bottom three bits are special; we leave them 0)
-       ltr(GD_TSS0);
+       ltr(GD_TSS0 + (id << 3));
 
        // Load the IDT
        lidt(&idt_pd);
```

Output:

```
check_kern_pgdir() succeeded!
check_page_free_list() succeeded!
check_page_installed_pgdir() succeeded!
SMP: CPU 0 found 4 CPU(s)
kernel panic on CPU 0 at kern/trap.c:349: kernel page fault!
```

Why the kernel still panics?

```
Welcome to the JOS kernel monitor!
Type 'help' for a list of commands.
K> backtrace
Stack backtrace:
  ebp f011ee90 eip f0100a4a args 00000001 f011eea8 00000000 f011ef34 f022ca80
        kern/monitor.c:142: monitor+308
  ...
  ebp f011ef70 eip f01042df args f011ef7c 00000000 00010094 f011efb8 f011ef9c
        kern/trapentry.S:105: <unknown>+0
  ebp f011efb8 eip f0105759 args fee00000 00001000 f011efd8 00010094 00010094
        kern/lapic.c:72: lapic_init+48
  ebp f011efd8 eip f01000d3 args f0105dcc 00001aac f011eff8 f01000a8 00000000
        kern/init.c:42: i386_init+55
  ebp f011eff8 eip f010003e args 00120021 00000000 00000000 00000000 00000000
        kern/entry.S:84: <unknown>+0
```

Keep in mind that the printed line is the *next instruction*. So we know this page fault happens when `i386_init` calls `lapic_init`, which calls `lapicw` and tries to write `lapic[index] = value`. 

Also, let's print out the faulted address in `trap.c` to get better idea:

```diff
@@ -341,7 +346,7 @@ page_fault_handler(struct Trapframe *tf)
 
        // LAB 3: Your code here.
        if ((tf->tf_cs & 3) == 0) {
-               panic("kernel page fault!\n");
+               panic("Page Fault in Kernel-Mode at %08x.\n", fault_va);
        }
```

Output:

```
SMP: CPU 0 found 4 CPU(s)
kernel panic on CPU 0 at kern/trap.c:349: Page Fault in Kernel-Mode at 0xef8030f0.
```

Refer to `memlayout.h`, we see the address is in `[MMIOBASE, MMIOLIM]`. As mentioned earlier, this area is for memory-mapped I/O for LAPIC. How could this fault? Why cannot kernel write to it? Then the answer is obvious: we didn't mark the page as writable in `mmio_map_region` previously in exercise3. 

```diff
@@ -643,7 +643,7 @@ mmio_map_region(physaddr_t pa, size_t size)
        uint32_t sz = ROUNDUP(size, PGSIZE);
        if (base + sz > MMIOLIM) panic("MMIOLIM overflowed!\n");
 
-       boot_map_region(kern_pgdir, base, sz, pa, PTE_PCD|PTE_PWT);
+       boot_map_region(kern_pgdir, base, sz, pa, PTE_PCD|PTE_PWT|PTE_W);
```

Output:

```
SMP: CPU 0 found 4 CPU(s)
enabled interrupts: 1 2
SMP: CPU 1 starting
SMP: CPU 2 starting
SMP: CPU 3 starting
[00000000] new env 00001000
kernel panic on CPU 0 at kern/trap.c:349: Page Fault in Kernel-Mode at 00000000.

K> backtrace
Stack backtrace:
  ebp f011ee18 eip f0100a4a args 00000001 f011ee30 00000000 f011eebc f022ca80
        kern/monitor.c:142: monitor+308
  ...
  ebp f011eef8 eip f01042dd args f011ef04 00000000 f01213c0 f011efa8 f011ef24
        kern/trapentry.S:105: <unknown>+0
  ebp f011efa8 eip f0104383 args f01213c0 00010094 f011efc8 f0104364 fee00000
        ./kern/spinlock.h:45: sched_halt+156
  ebp f011efc8 eip f01043c3 args 00010094 00000000 f011eff8 f0100194 f02228f8
        kern/sched.c:35: sched_yield+11
  ebp f011efd8 eip f0100194 args f02228f8 00000000 f011eff8 f0100139 00000000
        kern/init.c:97: mp_main+0
  ebp f011eff8 eip f010003e args 00120021 00000000 00000000 00000000 00000000
        kern/entry.S:84: <unknown>+0
```

Another page fault happens later when calling `sched_yield`, but the output matches the lab spec. Let's continue.

## Locking

> **Exercise 5.** Apply the big kernel lock as described above, by calling `lock_kernel()` and `unlock_kernel()` at the proper locations.

```diff
$ git diff
diff --git a/kern/env.c b/kern/env.c
index dbec1d5..d5d94b4 100644
--- a/kern/env.c
+++ b/kern/env.c
@@ -567,6 +567,7 @@ env_run(struct Env *e)
        curenv->env_runs += 1;
 
        lcr3(PADDR(e->env_pgdir));
+       unlock_kernel();
        env_pop_tf(&(e->env_tf));
 }
 
diff --git a/kern/init.c b/kern/init.c
index e5491ec..61bf47c 100644
--- a/kern/init.c
+++ b/kern/init.c
@@ -43,6 +43,7 @@ i386_init(void)
 
        // Acquire the big kernel lock before waking up APs
        // Your code here:
+       lock_kernel();
 
        // Starting non-boot CPUs
        boot_aps();
@@ -109,6 +110,8 @@ mp_main(void)
        // only one CPU can enter the scheduler at a time!
        //
        // Your code here:
+       lock_kernel();
+       sched_yield();
 
        // Remove this after you finish Exercise 6
        for (;;);
diff --git a/kern/trap.c b/kern/trap.c
index f209d8d..c0dbf19 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -301,6 +301,7 @@ trap(struct Trapframe *tf)
                // serious kernel work.
                // LAB 4: Your code here.
                assert(curenv);
+               lock_kernel();
 
                // Garbage collect if current enviroment is a zombie
                if (curenv->env_status == ENV_DYING) {
```

> **Question**
>
> 2. It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

2. Consider when user code makes system call with `INT` instruction, going through: `INT -> trapentry.S -> trap()`. Before entering `trap()`, some register values would have already been pushed to the stack. However, the big kernel lock is not required until in `trap()`. Thus, if multiple CPUs are sharing a kernel stack, the values pushed onto the kernel stack before acquiring the lock would mess up.



## Round-Robin Scheduling

> **Exercise 6.** Implement round-robin scheduling in `sched_yield()` as described above. Don't forget to modify `syscall()` to dispatch `sys_yield()`.
>
> Make sure to invoke `sched_yield()` in `mp_main`.
>
> Modify `kern/init.c` to create three (or more!) environments that all run the program `user/yield.c`.

```diff
$ git diff
diff --git a/kern/init.c b/kern/init.c
index 61bf47c..9ac22d7 100644
--- a/kern/init.c
+++ b/kern/init.c
@@ -53,7 +53,9 @@ i386_init(void)
        ENV_CREATE(TEST, ENV_TYPE_USER);
 #else
        // Touch all you want.
-       ENV_CREATE(user_primes, ENV_TYPE_USER);
+       ENV_CREATE(user_yield, ENV_TYPE_USER);
+       ENV_CREATE(user_yield, ENV_TYPE_USER);
+       ENV_CREATE(user_yield, ENV_TYPE_USER);
 #endif // TEST*
 
        // Schedule and run the first user environment!
@@ -114,7 +116,7 @@ mp_main(void)
        sched_yield();
 
        // Remove this after you finish Exercise 6
-       for (;;);
+       // for (;;);
 }
 
 /*
diff --git a/kern/sched.c b/kern/sched.c
index f595bb1..fd38132 100644
--- a/kern/sched.c
+++ b/kern/sched.c
@@ -29,6 +29,23 @@ sched_yield(void)
        // below to halt the cpu.
 
        // LAB 4: Your code here.
+       static int prev_run = -1;
+       struct Env *e;
+
+       for (int i = (prev_run + 1) % NENV; i != prev_run; i = (i + 1) % NENV) {
+               e = &envs[i];
+               if (e->env_status == ENV_RUNNABLE) {
+                       prev_run = i;
+                       env_run(e);
+                       // env_run doesn't return
+               }
+       }
+
+       // No other RUNNABLE env, except the previously run
+       if (prev_run >= 0 && envs[prev_run].env_status == ENV_RUNNING) {
+               e = &envs[prev_run];
+               env_run(e);
+       }
 
        // sched_halt never returns
        sched_halt();
diff --git a/kern/syscall.c b/kern/syscall.c
index ddc8726..8453e7e 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -288,6 +288,9 @@ syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
        case SYS_env_destroy:
                sys_env_destroy(a1);
                return 0;
+       case SYS_yield:
+               sys_yield();
+               return 0;
        case NSYSCALLS:
        default:
                return -E_INVAL;
diff --git a/lib/libmain.c b/lib/libmain.c
index fc42204..2dda2ac 100644
--- a/lib/libmain.c
+++ b/lib/libmain.c
@@ -16,7 +16,7 @@ libmain(int argc, char **argv)
        envid_t eid = sys_getenvid();
        for (int i = 0; i < 1024; ++i) {
                if (envs[i].env_id == eid) {
-                       thisenv = &envs[0];
+                       thisenv = &envs[i];
                        break;
                }
        }
```

> **Question**
>
> 3. In your implementation of `env_run()` you should have called `lcr3()`. Before and after the call to `lcr3()`, your code makes references (at least it should) to the variable `e`, the argument to `env_run`. Upon loading the `%cr3` register, the addressing context used by the MMU is instantly changed. But a virtual address (namely `e`) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer `e` be dereferenced both before and after the addressing switch?
>
> 4. Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

3. `e` refers to a `struct Env` in `envs`. This is mapped in the address space of all envs at `UENVS`.

4. For future resumption. The registers are saved in the trapframe when trapping into kernel. In `trap.c:trap`, `cur->env_tf = *tf` saves the trapframe. The trapframe is pushed onto the stack in `trapentry.S`, and restored in `trap.c:env_pop_tf`.

   One possible question is that, in JOS, `trapentry.S` builds the trapframe. In xv6, `trapasm.S` builds the trapframe, but `swtch` also builds a `struct context`. Why JOS doesn't do something similar?

   In xv6, the scheduler thread plays a role in scheduling. The reason for a separate scheduling thread is "it's sometimes not safe for it to execute on any process's kernel stack".

   ```
   Proc1_user --> Proc1_kernel --> schedulerThread --> Proc2_kernel --> Proc2_user 
   		       ^				^					^				  ^
   		       |				|					|				  |
   		     trapframe		  context			  context			trapframe
   ```

    A possible point of confusion is "thread" and "process" term in xv6. The "user environmen", `struct Env`, in JOS is the same as "process", `struct proc`, in xv6, which is similar to the common meaning of Linux process. How about the "user thread" and "kernel thread"? Why there's no such thing as `struct Thread`?
    
    The *thread* here is different from a Linux thread, or a `pthread` in C, which the user can manipulate. The thread here means "a thread of execution" and a process always have a user thread and a kernel thread. The thread is switched mainly by saving/restoring registers and switching stacks. For example, when making a system call, registers are saved/restored with `struct Trapframe`, and the process switches from user stack to kernel stack. In other words, "user thread" and "kernel thread" in xv6 just means the same process, but executing in user space/kernel space. This is readily supported, since we can already switch between user mode and kernel mode. Thus, no `struct Thread` is needed. This are just (possibly) confusing terms.  

    The saving/restoring of trapframe handles the transition between user/kernel space, while saving/restoring of `struct context` handles transition between user proc and scheduler. The fact that we are transition using assembly code, `swtch.S`, instead of a function call, makes saving/restoring registers necessary.

    In JOS, no scheduler thread is involved. Also, the scheduling is all done in C: `sys_yield` calls `sched_yield`, which searchs the `envs` for another runnable env.

    ```
   Proc1_user --> Proc1_kernel --> Proc2_kernel --> Proc2_user 
   		    ^								  ^
   		    |								  |
   		  trapframe	        			trapframe
   ```

    One subtle (and interesting) issue related to multiprocess scheduling is the state of kernel stack. We know kernel stack has two main usages: (1) pushing registers when trapping into kernel mode (and restoring when returnning into user mode); (2) running kernel code. What does the stack look like just after trapping into kernel space, and what does it look like just after returning back to user space?

    When just trapping into kernel mode, the top of kernel stack contains `struct Trapframe` and then stack frames for function call to `trap`. `env->env_tf` is set to point to the `struct Trapframe` for later use. Then the kernel code executes on the kernel stack. When the current process is to be de-scheduled: `sched_yield` -> `env->run` -> `env_pop_tf(env->env_tf)`. The definition of `env_pop_tf` is:   
    ```C
    void env_pop_tf(struct Trapframe *tf) {
        // Record the CPU we are running on for user-space debugging
        curenv->env_cpunum = cpunum();

        asm volatile(
            "\tmovl %0,%%esp\n"
            "\tpopal\n"
            "\tpopl %%es\n"
            "\tpopl %%ds\n"
            "\taddl $0x8,%%esp\n" /* skip tf_trapno and tf_errcode */
            "\tiret\n"
            : : "g" (tf) : "memory");
        panic("iret failed");  /* mostly to placate the compiler */
    }
    ```
    `%0` refers to the argument `tf`. So setting `%esp` to be `tf` restores the kernel stack to the initial state (when just trapping into kernel mode) completely.

## System Calls for Environment Creation

`Fork()` is a system call in Unix, while JOS provides more primitive set of syscalls such that `fork()` can be implemented completely in user space. Check `user/dumbfork.c:dumbfork`.  

> **Exercise 7.** Implement the system calls described above in `kern/syscall.c` and make sure `syscall()` calls them. You will need to use various functions in `kern/pmap.c` and `kern/env.c`, particularly `envid2env()`. For now, whenever you call `envid2env()`, pass 1 in the `checkperm` parameter. Be sure you check for any invalid system call arguments, returning `-E_INVAL` in that case.

```diff
$ git diff
diff --git a/kern/syscall.c b/kern/syscall.c
index 8453e7e..81d673f 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -12,6 +12,10 @@
 #include <kern/console.h>
 #include <kern/sched.h>
 
+bool aligned(void *addr) {
+       return ROUNDUP(addr, PGSIZE) == addr;
+}
+
 // Print a string to the system console.
 // The string is exactly 'len' characters long.
 // Destroys the environment on memory errors.
@@ -85,7 +89,18 @@ sys_exofork(void)
        // will appear to return 0.
 
        // LAB 4: Your code here.
-       panic("sys_exofork not implemented");
+       struct Env* e;
+       int res;
+       if ((res = env_alloc(&e, curenv->env_id)) < 0)
+               return res;
+
+       e->env_status = ENV_NOT_RUNNABLE;
+
+       // Copy register set, constructing return value of 0
+       memcpy(&(e->env_tf), &(curenv->env_tf), sizeof(struct Trapframe));
+       e->env_tf.tf_regs.reg_eax = 0;
+
+       return e->env_id;
 }
 
 // Set envid's env_status to status, which must be ENV_RUNNABLE
@@ -105,7 +120,16 @@ sys_env_set_status(envid_t envid, int status)
        // envid's status.
 
        // LAB 4: Your code here.
-       panic("sys_env_set_status not implemented");
+       if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
+               return -E_INVAL;
+
+       struct Env *e;
+       int res;
+       if ((res = envid2env(envid, &e, 1)) < 0)
+               return res;
+
+       e->env_status = status;
+       return 0;
 }
 
 // Set the page fault upcall for 'envid' by modifying the corresponding struct
@@ -150,7 +174,31 @@ sys_page_alloc(envid_t envid, void *va, int perm)
        //   allocated!
 
        // LAB 4: Your code here.
-       panic("sys_page_alloc not implemented");
+       if ((uint32_t) va >= UTOP || !aligned(va))
+               return -E_INVAL;
+
+       if (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL))
+               return -E_INVAL;
+
+       struct Env *e;
+       struct PageInfo *allocated;
+       pte_t *pte;
+       int res;
+
+       if ((res = envid2env(envid, &e, 1)) < 0)
+               return res;
+
+       allocated = page_alloc(1);
+       if (allocated == NULL)
+               return -E_NO_MEM;
+
+       // If no page mapped originally, page_remove does nothing
+       page_remove(e->env_pgdir, va);
+
+       if ((res = page_insert(e->env_pgdir, allocated, va, perm)) < 0)
+               return res;
+
+       return 0;
 }
 
 // Map the page of memory at 'srcva' in srcenvid's address space
@@ -181,7 +229,38 @@ sys_page_map(envid_t srcenvid, void *srcva,
        //   check the current permissions on the page.
 
        // LAB 4: Your code here.
-       panic("sys_page_map not implemented");
+
+       if ((uint32_t)srcva >= UTOP || !aligned(srcva) || (uint32_t)dstva >= UTOP || !aligned(dstva))
+               return -E_INVAL;
+
+       if (!(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL))
+               return -E_INVAL;
+
+       struct Env *src_env, *dst_env;
+       struct PageInfo *pp;
+       pte_t *pte;
+       int res;
+
+       if (envid2env(srcenvid, &src_env, 1) < 0)
+               return -E_BAD_ENV;
+
+       if (envid2env(dstenvid, &dst_env, 1) < 0)
+               return -E_BAD_ENV;
+
+       pp = page_lookup(src_env->env_pgdir, srcva, &pte);
+
+       // not mapped
+       if (pp == NULL)
+               return -E_INVAL;
+
+       // illegal perm
+       if ((perm & PTE_W) && ((*pte & PTE_W) == 0))
+               return -E_INVAL;
+
+       if ((res = page_insert(dst_env->env_pgdir, pp, dstva, perm)) < 0)
+               return res;
+
+       return 0;
 }
 
 // Unmap the page of memory at 'va' in the address space of 'envid'.
@@ -197,7 +276,15 @@ sys_page_unmap(envid_t envid, void *va)
        // Hint: This function is a wrapper around page_remove().
 
        // LAB 4: Your code here.
-       panic("sys_page_unmap not implemented");
+       if ((uint32_t)va >= UTOP || !aligned(va))
+               return -E_INVAL;
+
+       struct Env *e;
+       if (envid2env(envid, &e, 1) < 0)
+               return E_BAD_ENV;
+
+       page_remove(e->env_pgdir, va);
+       return 0;
 }
 
 // Try to send 'value' to the target env 'envid'.
@@ -278,20 +365,25 @@ syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
        case SYS_env_destroy:
                sys_env_destroy(a1);
                return 0;
+       case SYS_page_alloc:
+               return sys_page_alloc(a1, (void *)a2, a3);
+       case SYS_page_map:
+               return sys_page_map(a1, (void *)a2, a3, (void *)a4, a5);
+       case SYS_page_unmap:
+               return sys_page_unmap(a1, (void *)a2);
+       case SYS_exofork:
+               return sys_exofork();
+       case SYS_env_set_status:
+               return sys_env_set_status(a1, a2);
        case SYS_yield:
                sys_yield();
                return 0;
-       case NSYSCALLS:
        default:
                return -E_INVAL;
        }
```

