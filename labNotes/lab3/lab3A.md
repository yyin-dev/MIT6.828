# Lab3 Part A: User Envrionment and Exception Handling

> **Exercise 1.** Modify `mem_init()` in `kern/pmap.c` to allocate and map the `envs` array. This array consists of exactly `NENV` instances of the `Env` structure allocated much like how you allocated the `pages` array. Also like the `pages` array, the memory backing `envs` should also be mapped user read-only at `UENVS` (defined in `inc/memlayout.h`) so user processes can read from this array.
>
> You should run your code and make sure `check_kern_pgdir()` succeeds.

Easy. Very similar to `pages` in Lab2.



> **Exercise 2.** In the file `env.c`, finish coding the following functions:
>
> - `env_init()`
>
>   Initialize all of the `Env` structures in the `envs` array and add them to the `env_free_list`. Also calls `env_init_percpu`, which configures the segmentation hardware with separate segments for privilege level 0 (kernel) and privilege level 3 (user).
>
> - `env_setup_vm()`
>
>   Allocate a page directory for a new environment and initialize the kernel portion of the new environment's address space.
>
> - `region_alloc()`
>
>   Allocates and maps physical memory for an environment
>
> - `load_icode()`
>
>   You will need to parse an ELF binary image, much like the boot loader already does, and load its contents into the user address space of a new environment.
>
> - `env_create()`
>
>   Allocate an environment with `env_alloc` and call `load_icode` to load an ELF binary into it.
>
> - `env_run()`
>
>   Start a given environment running in user mode.
>
> As you write these functions, you might find the new cprintf verb `%e` useful -- it prints a description corresponding to an error code. For example,
>
> ```c
> 	r = -E_NO_MEM;
> 	panic("env_alloc: %e", r);
> ```
>
> will panic with the message "env_alloc: out of memory".

- `env_setup_vm`: use `kern_pgdir` as a template. Questions:  
    - Why we only need to copy `kern_pgdir`, the page directory of the kernel? How about the page tables pointed to by the page directory?   
    Answer: note that both page directory and page tables stores *physical addresses*, so duplicating the page directory means that page tables for kernel memory are **shared** among different user environments. 

    - `kern_pgdir` is duplicated into user environment's page directory when the user environment is created. What if the kernel part of the page table is modified later (by the kernel code)? How to sync this change to the page tables of existing user environments?  
    Answer: This can never happen. Consider the memory layout of a user environment in JOS. In `pamp.c/mem_init`, all physical memory is mapped at `[KERNBASE, 0xFFFFFFFF]` into kernel's address space. Page fault can happen in two cases: (1) the memory is purged to disk; (2) the memory is not mapped. As JOS doesn't support purging memory content to disk, so page fault only happens when the memory is not mapped. However, since all physical memory is mapped into kernel's address space, page fault can never happen in kernel mode! Actually, check `page_fault_handler` in `trap.c`, page fault in kernel mode is considered a bug and the kernel panics.  

        Lab3B exercise 9 explains why page fault should never happen in kernel mode from another perspective: if page fault happens when manipulating kernel's own data structures, this is a bug and should panic; the kernel may also need to dereferencing memory address provided by user. So kernel should always check/verify the address before using it, and take whatever actions is needed (kill ths process? dynamically map a new page?).  

        In short, 3 reasons why page fault shouldn't happen in kernel mode:  

        - All physical memory is mapped into kernel's address space above `KERNBASE`;  

        - If page fault happens when manipulating kernel's own data strcture, this is a bug;  
        
        - Before using address in user's address space, kernel should check if the address is valid and take suitable actions.       

        Side note: As `KERNBASE = 0xF0000000`, this limits the size of physical memory that can be used, 256MB. This also means that a user environment can use at most 256MB of physical memory, so user address space is at most 256MB. 

- `load_icode`: Maybe the hardest part in this exercise.
  - loading sections is very similar to boot loader in `boot/main.c`. 
  - Switching page directory is a clever trick.
  - Setting up `e->env_tf` to control execution start point.

- `env_run`: the given comment is not 100% right.

```diff
$ git diff
diff --git a/kern/env.c b/kern/env.c
index db2fda9..32e5315 100644
--- a/kern/env.c
+++ b/kern/env.c
@@ -116,6 +116,13 @@ env_init(void)
 {
        // Set up envs array
        // LAB 3: Your code here.
+       for (int i = NENV - 1; i >= 0; --i) {
+               struct Env *env = &envs[i];
+               env->env_status = ENV_FREE;
+               env->env_id = 0;
+               env->env_link = env_free_list;
+               env_free_list = env;
+       }
 
        // Per-CPU part of the initialization
        env_init_percpu();
@@ -180,6 +187,13 @@ env_setup_vm(struct Env *e)
 
        // LAB 3: Your code here.
 
+       // Duplicate the page directory
+       e->env_pgdir = page2kva(p); // e->env_pgdir requries kernel virtual address
+       memcpy(e->env_pgdir, kern_pgdir, PGSIZE);
+
+       // Increment ref count
+       p->pp_ref += 1;
+
        // UVPT maps the env's own page table read-only.
        // Permissions: kernel R, user R
        e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;
@@ -267,6 +281,22 @@ region_alloc(struct Env *e, void *va, size_t len)
        //   'va' and 'len' values that are not page-aligned.
        //   You should round va down, and round (va + len) up.
        //   (Watch out for corner-cases!)
+       if (e == NULL) panic("region_alloc: e is NULL\n");
+       if (len == 0) return;
+
+       uintptr_t va_start = (uintptr_t) ROUNDDOWN(va, PGSIZE);
+       uintptr_t va_end = (uintptr_t) ROUNDUP((uint32_t)va + len, PGSIZE);
+       uint32_t length = va_end - va_start;
+       uint32_t npages = len / PGSIZE;
+
+       uintptr_t vaddr;
+       for (vaddr = va_start; vaddr < va_end; vaddr += PGSIZE) {
+               struct PageInfo *pp;
+               if ((pp = page_alloc(0)) == NULL) 
+                       panic("region_alloc: page_alloc fails\n");
+               if (page_insert(e->env_pgdir, pp, (void *)vaddr, PTE_U | PTE_W) < 0) 
+                       panic("region_alloc: page_insert fails\n");
+       }
 }
 
 //
@@ -324,10 +354,41 @@ load_icode(struct Env *e, uint8_t *binary)
 
        // LAB 3: Your code here.
 
+       // Switch to env's page directory.
+       // We need to copy into the memory space of the env. After the switch,
+       // virtual address are translated using e->env_pgdir.
+       lcr3(PADDR(e->env_pgdir));
+
+       struct Proghdr *ph, *eph;
+       struct Elf *elfhdr = (struct Elf *)binary;
+
+       ph = (struct Proghdr *) ((uint8_t *)elfhdr + elfhdr->e_phoff);
+       eph = ph + elfhdr->e_phnum;
+       for (; ph < eph; ++ph) {
+               if (ph->p_type == ELF_PROG_LOAD) {
+                       // The hint says that:
+                       // ph->p_filesz from binary + ph->p_offset should be copied to ph->p_va;
+                       // Remaining bytes indicated by the header should be zeroed.
+                       region_alloc(e, (void *)ph->p_va, ph->p_memsz);
+                       memset((void *)ph->p_va, 0, ph->p_memsz);
+                       memcpy((void *)ph->p_va, (void *) binary + ph->p_offset, ph->p_filesz);
+               }
+       }
+
+       // Switch back to kernel's page directory
+       lcr3(PADDR(kern_pgdir));
+
        // Now map one page for the program's initial stack
        // at virtual address USTACKTOP - PGSIZE.
 
        // LAB 3: Your code here.
+       region_alloc(e, (void *) USTACKTOP - PGSIZE, PGSIZE);
+
+       // Set up e->env_tf to ensure the entry point is executed at the beginning.
+       // e->env_tf would be poped by env_pop_tf() in env_run() before returning to
+       // user space.
+       e->env_tf.tf_eip = elfhdr->e_entry;
+       e->env_tf.tf_esp = USTACKTOP;
 }
 
 //
@@ -341,6 +402,16 @@ void
 env_create(uint8_t *binary, enum EnvType type)
 {
        // LAB 3: Your code here.
+       if (env_free_list == NULL)
+               panic("env_create: env_free_list is NULL\n");
+
+       struct Env *env;
+       if (env_alloc(&env, 0) < 0) {
+               panic("env_create: env_alloc fails\n");
+       }
+
+       load_icode(env, binary);
+       env->env_type = type;
 }
 
 //
@@ -458,6 +529,18 @@ env_run(struct Env *e)
 
        // LAB 3: Your code here.
 
-       panic("env_run not yet implemented");
+       // Note: the actual implementation is a little different from
+       // the provided hint in the comment.
+       if (curenv) {
+               if (curenv->env_status == ENV_RUNNING) {
+                       curenv->env_status = ENV_RUNNABLE;
+               }
+       }
+       curenv = e;
+       curenv->env_status = ENV_RUNNING;
+       curenv->env_runs += 1;
+
+       lcr3(PADDR(e->env_pgdir));
+       env_pop_tf(&(e->env_tf));
 }
```



> **Exercise 3.** Read [Chapter 9, Exceptions and Interrupts](https://pdos.csail.mit.edu/6.828/2018/readings/i386/c09.htm) in the [80386 Programmer's Manual](https://pdos.csail.mit.edu/6.828/2018/readings/i386/toc.htm) (or Chapter 5 of the [IA-32 Developer's Manual](https://pdos.csail.mit.edu/6.828/2018/readings/ia32/IA32-3A.pdf)), if you haven't already.

Refer to `IntelManual - Chapter9.md`.



### Basics of Protected Control Transfer

Exceptions and interrupts are "protected control transfers," which cause the processor to switch from user to kernel mode without giving the user code any opportunity to interfere.

To provide protection:

- Interrupte descriptor table (IDT)
  The processor ensures that interrupts and exceptions can only cause the kernel to be entered at a few entry-points determined by the kernel itself.
  The x86 allows up to 256 different interrupt or exception entry points, each with a different interrupt *vector*. **A vector is a number** between 0 and 255. A interrupt's vector is determined by the source of interrupt. The CPU uses the vector as an index into the processor's interrupt descriptor table (IDT). From the entry in IDT the processor loads:

  - the value for %EIP, pointing to the handler code in kernel.
  - the value for %cs, including the privilege level.

- The Task State Segment (TSS)
  Before executing the handler, the processor needs to save the *old* processor state ike %eip and %cs for later resumption. This should be stored on the kernel stack, not user stack. 

  A *task state segment* (TSS) specifies the segment selector and address of the kernel stack. The processor pushes %ss, %esp, %eflags, %cs, %eip, and an optional error code, onto the kernel stack. Then it loads %cs, %eip from the interrupt descriptor in the IDT, and sets %esp and %ss to refer to the new stack. The fields in TSS is `SS0` and `ESP0`, since the handler executes with `CPL = 0`.

  In JOS, TSS is only used to define the kernel stack that the kernel should switch to when it transfers from user mode to kernel mode. 

### Types of Exceptions and Interrupts

All synchronous exceptions that the x86 processor can generate use interrupt vectors 0 - 31. Interrupt vectors greater than 31 are only used by *software interrupts*, which can be generated by the `INT` instruction, or asynchronous *hardware interrupts*, caused by external devices when they need attention.

### An example

Let's say the processor is executing code in a user mode and encounters a divide by zero.

1. The processor switches to stack defined by `SS0` and `ESP0` of the TSS, having value `GD_KD` and `KSTACKTOP` in JOS.

2. Pushes to the kernel stack, starting at address `KSTACKTOP`:

   ```
                        +--------------------+ KSTACKTOP             
                        | 0x00000 | old SS   |     " - 4
                        |      old ESP       |     " - 8
                        |     old EFLAGS     |     " - 12
                        | 0x00000 | old CS   |     " - 16
                        |      old EIP       |     " - 20 <---- ESP 
                        +--------------------+      
   ```

3. Divide error is interrupt vector 0 on x86. The processor reads IDT descriptor and sets `CS:EIP` to the handler function. 

4. The hadler function executes.

For certain types of x86 exceptions, in addition to the "standard" five words above, the processor pushes onto the stack another word containing an *error code*.

```
                     +--------------------+ KSTACKTOP             
                     | 0x00000 | old SS   |     " - 4
                     |      old ESP       |     " - 8
                     |     old EFLAGS     |     " - 12
                     | 0x00000 | old CS   |     " - 16
                     |      old EIP       |     " - 20
                     |     error code     |     " - 24 <---- ESP
                     +--------------------+ 
```

### Nested Exceptions and Interrupts

The processor can take exceptions and interrupts both from kernel and user mode. Stack switching happens only when entering kernel mode from the user mode. If the processor is *already* in kernel mode when the interrupt or exception occurs, the CPU just pushes values on the same kernel stack. In this way, the kernel can gracefully handle *nested exceptions* caused by code within the kernel itself.

If the processor is already in kernel mode and takes an exception, it doesn't save the old `SS` or `ESP` registers. For exception types that do not push an error code, the kernel stack looks like the following on entry to the handler.

```
                     +--------------------+ <---- old ESP
                     |     old EFLAGS     |     " - 4
                     | 0x00000 | old CS   |     " - 8
                     |      old EIP       |     " - 12
                     +--------------------+             
```

Corner-case: If the processor takes an exception while already in kernel mode, and *cannot push its old state onto the kernel stack* for any reason such as lack of stack space, then there is nothing the processor can do to recover, so it simply resets itself.

### Setting up the IDT

Each exception or interrupt should have its own handler in `trapentry.S` and `trap_init()` should initialize the IDT with the addresses of these handlers. Each of the handlers should build a `struct Trapframe` (see `inc/trap.h`) on the stack and call `trap()` (in `trap.c`) with a pointer to the Trapframe. `trap()` then handles the exception/interrupt or dispatches to a specific handler function.

> **Exercise 4.** Edit `trapentry.S` and `trap.c` and implement the features described above. The macros `TRAPHANDLER` and `TRAPHANDLER_NOEC` in `trapentry.S` should help you, as well as the T_* defines in `inc/trap.h`. You will need to add an entry point in `trapentry.S` (using those macros) for each trap defined in `inc/trap.h`, and you'll have to provide `_alltraps` which the `TRAPHANDLER` macros refer to. You will also need to modify `trap_init()` to initialize the `idt` to point to each of these entry points defined in `trapentry.S`; the `SETGATE` macro will be helpful here.
>
> Your `_alltraps` should:
>
> 1. push values to make the stack look like a struct Trapframe
> 2. load `GD_KD` into `%ds` and `%es`
> 3. `pushl %esp` to pass a pointer to the Trapframe as an argument to trap()
> 4. `call trap` (can `trap` ever return?)
>
> Consider using the `pushal` instruction; it fits nicely with the layout of the `struct Trapframe`.
>
> Test your trap handling code using some of the test programs in the `user` directory that cause exceptions before making any system calls, such as `user/divzero`. You should be able to get make grade to succeed on the `divzero`, `softint`, and `badsegment` tests at this point.

This exercise is not hard, if you read the manual in Exercise 3 carefully and you fully undersatnd how the interrupt handling in xv6 works. In particular, you should be clear about what hardware and software does respectively in the process.

The detail is in `../xv6-book-notes/3 - Traps, interrrupts, and drivers.md`, but here's a short summary. The structure of `struct Trapframe`:
```C
struct Trapframe {
	struct PushRegs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	uint32_t tf_trapno;
	/* below here defined by x86 hardware */
	uint32_t tf_err;
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_padding3;
	uint32_t tf_eflags;
	/* below here only when crossing rings, such as from user to kernel */
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_padding4;
} __attribute__((packed));
```
`trap_init` sets up entry point for interrupts/exceptions. When exception/interrupt happens: 
- The hardware reads from TSS to switches to new stack;
- The hardware saves %ss, %esp, %cs, %eip, and possibly an error code to the kernel stack, and updates %ss and %esp to refer to the kernel stack. This constructs part of the `struct Trapframe.`
- Control passes to the entry point in `trapentry.S`, pushes other registers onto the kernel stack to complete the `struct Trapframe`. Then calls `trap`, with the trapframe as an argument, in `_alltraps`.
- The `trap` function in `trap.c` executes. 
- In xv6, the trap handler returns and `trapret` pops argument from the stack and calls `iret` to restore the registers. In JOS, `trap` calls `trap_dispatch` to dispatch to specific handler. After that `trap` calls `env_run` to pop registers and fall back into user mode. Thus, for JOS, no `trapret` is needed. But both xv6 and JOS pops registers from the kernel stack and falls back into the user mode.

For xv6, `vectors.pl` generates `vectors.S`, the vector entries to the handlers. In `trap.c`, `tvinit` sets up the IDT. When interrupt happens, hardware switches stack, pushes some register values onto the kernel stack, and jumps to the vector entry.

For JOS, vector entries are generated manually using the two macros in `trapentry.S`, using constants defined in `inc/trap.h`. `kern/trap.c` sets up the IDT in a similar way as `tvinit` in xv6. `_alltraps` is also defined in `trapentry.S`, which is extremely similar to `trapentry.S` in xv6. The only difference is that `struct Trapframe` is defined differently in xv6 and JOS, so some registers are pushed in xv6 but not in JOS. Though implemented differently, the idea is exactly the same.

```diff
$ git diff
diff --git a/kern/trap.c b/kern/trap.c
index e27b556..3cd38a9 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -25,6 +25,25 @@ struct Pseudodesc idt_pd = {
        sizeof(idt) - 1, (uint32_t) idt
 };
 
+/* Declaraions for vector entries in trapentry.S */
+void DIVIDE();
+void DEBUG();
+void NMI();
+void BRKPT();
+void OFLOW();
+void BOUND();
+void ILLOP();
+void DEVICE();
+void DBLFLT();
+void TSS();
+void SEGNP();
+void STACK();
+void GPFLT();
+void PGFLT();
+void FPERR();
+void ALIGN();
+void MCHK();
+void SIMDERR();
 
 static const char *trapname(int trapno)
 {
@@ -66,6 +85,30 @@ trap_init(void)
 
        // LAB 3: Your code here.
 
+       // Define default gate for reserved/unused vector
+       for (int i = 0; i < 32; ++i) SETGATE(idt[i], 0, GD_KT, 0, 0);
+
+       // Refer to IA32-3A 5.3.1 Table 5-1 for isTrap
+       // Trap: 1, 3, 4
+       SETGATE(idt[T_DIVIDE], 0, GD_KT, DIVIDE, 0);
+       SETGATE(idt[T_DEBUG], 1, GD_KT, DEBUG, 0);
+       SETGATE(idt[T_NMI], 0, GD_KT, NMI, 0);
+       SETGATE(idt[T_BRKPT], 1, GD_KT, BRKPT, 0);
+       SETGATE(idt[T_OFLOW], 1, GD_KT, OFLOW, 0);
+       SETGATE(idt[T_BOUND], 0, GD_KT, BOUND, 0);
+       SETGATE(idt[T_ILLOP], 0, GD_KT, ILLOP, 0);
+       SETGATE(idt[T_DEVICE], 0, GD_KT, DEVICE, 0);
+       SETGATE(idt[T_DBLFLT], 0, GD_KT, DBLFLT, 0);
+       SETGATE(idt[T_TSS], 0, GD_KT, TSS, 0);
+       SETGATE(idt[T_SEGNP], 0, GD_KT, SEGNP, 0);
+       SETGATE(idt[T_STACK], 0, GD_KT, STACK, 0);
+       SETGATE(idt[T_GPFLT], 0, GD_KT, GPFLT, 0);
+       SETGATE(idt[T_PGFLT], 0, GD_KT, PGFLT, 0);
+       SETGATE(idt[T_FPERR], 0, GD_KT, FPERR, 0);
+       SETGATE(idt[T_ALIGN], 0, GD_KT, ALIGN, 0);
+       SETGATE(idt[T_MCHK], 0, GD_KT, MCHK, 0);
+       SETGATE(idt[T_SIMDERR], 0, GD_KT, SIMDERR, 0);
+
        // Per-CPU setup 
        trap_init_percpu();
 }
diff --git a/kern/trapentry.S b/kern/trapentry.S
index 22fc640..07e1480 100644
--- a/kern/trapentry.S
+++ b/kern/trapentry.S
@@ -47,9 +47,63 @@
  * Lab 3: Your code here for generating entry points for the different traps.
  */
 
+# Refer to IA32-3A 5.3.1 Table 5-1 for whether the processor pushes error code.
+# Interrupt/Exception that pushes error code: 8, 10 - 14, 17.
+
+# 0 - 8
+TRAPHANDLER_NOEC(DIVIDE, T_DIVIDE)
+TRAPHANDLER_NOEC(DEBUG, T_DEBUG)
+TRAPHANDLER_NOEC(NMI, T_NMI)
+TRAPHANDLER_NOEC(BRKPT, T_BRKPT)
+TRAPHANDLER_NOEC(OFLOW, T_OFLOW)
+TRAPHANDLER_NOEC(BOUND, T_BOUND)
+TRAPHANDLER_NOEC(ILLOP, T_ILLOP)
+TRAPHANDLER_NOEC(DEVICE, T_DEVICE)
+TRAPHANDLER(DBLFLT, T_DBLFLT)
+
+# 10 - 14
+TRAPHANDLER(TSS, T_TSS)
+TRAPHANDLER(SEGNP, T_SEGNP)
+TRAPHANDLER(STACK, T_STACK)
+TRAPHANDLER(GPFLT, T_GPFLT)
+TRAPHANDLER(PGFLT, T_PGFLT)
+
+# 16 - 19
+TRAPHANDLER_NOEC(FPERR, T_FPERR)
+TRAPHANDLER(ALIGN, T_ALIGN)
+TRAPHANDLER_NOEC(MCHK, T_MCHK)
+TRAPHANDLER_NOEC(SIMDERR, T_SIMDERR)
 
 
 /*
  * Lab 3: Your code here for _alltraps
  */
 
+# Very similar to xv6-public/trapasm.S
+.globl _alltraps
+_alltraps:
+  # Build trap frame.
+  pushl %ds
+  pushl %es
+  pushal       # Construt `struct PushRegs`
+  
+  # Set segment selectors:
+  # (1) The processor sets %cs and %ss 
+  # (2) alltraps sets up data segments by setting %ds and %es.
+  movw $(GD_KD), %ax
+  movw %ax, %ds
+  movw %ax, %es
+
+  # Call trap(tf), where tf=%esp
+  pushl %esp # pointing to a 'struct trapframe`, as an arg to trap
+  call trap
+  addl $4, %esp # Pop the argument
+
+# Return falls through to trapret...
+.globl trapret
+trapret:
+  popal
+  popl %es
+  popl %ds
+  addl $0x8, %esp  # trapno and errcode
+  iret # Pops %cs, $eip, %flags, %esp, %ss from stack
```

> **Questions**
>
> 1. What is the purpose of having an individual handler function for each exception/interrupt? (i.e., if all exceptions/interrupts were delivered to the same handler, what feature that exists in the current implementation could not be provided?)
> 2. Did you have to do anything to make the `user/softint` program behave correctly? The grade script expects it to produce a general protection fault (trap 13), but `softint`'s code says `int $14`. *Why* should this produce interrupt vector 13? What happens if the kernel actually allows `softint`'s `int $14` instruction to invoke the kernel's page fault handler (which is interrupt vector 14)?

1. Cannot impose protection/isolation. For example, only syscall can be generated by user with `int`.
2. Nothing special needed. `13` is generation protection fault. Since `softint` tries to invoke a page fault handler, which shouldn't be allowed, a protection fault is generated. If allowed, this's a safety error.
