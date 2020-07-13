# Lab4 Part B - Copy-on-Write Fork



Naive `fork` copies the entire address space, which is inefficient. Later versions of Unix used virtual memory hardware to allow the parent and child to *share* the memory mapped into their own address spaces until one of the processes modifies it. This technique is known as *copy-on-write*. To do this, on `fork()` the kernel would copy the address space *mappings* from the parent to the child instead of the contents of the mapped pages, and mark the now-shared pages read-only. When one of the two processes tries to write to one of these shared pages, the process takes a page fault. At this point, the Unix kernel realizes that the page was really a "virtual" or "copy-on-write" copy, and so it makes a new, private, writable copy of the page for the faulting process. In this way, the contents of individual pages aren't actually copied until they are actually written to. This optimization makes a `fork()` followed by an `exec()` in the child much cheaper: the child will probably only need to copy one page (the current page of its stack) before it calls `exec()`.

We'll implement `fork()` with copy-on-write support in user-space, leaving the kernel simple and allowing for flexibility. 

## User-level page fault handling

Copy-on-write is only one of many possible uses for user-level page fault handling.

It's common to set up an address space so that page faults indicate when some action needs to take place. For example, most Unix kernels initially map only a single page for a new process's stack, and allocate and map additional stack pages "on demand" as the process's stack grows. The kernel must keep track of what action to take when a page fault occurs in each region of the address space. For example, a fault in the stack region will allocate and map new page of physical memory. A fault in the BSS region will typically allocate a new page, fill it with zeroes, and map it. In systems with demand-paged executables, a fault in the text region will read a page of the binary off of disk and then map it.

To acheive the feature above, the kernel needs to keep track of a lot of information. For JOS, action for page fault would be decided in the user space. 

### Setting the Page Fault Handler

The user environment registers a *page fault handler entrypoint* with the JOS kernel using `sys_env_set_pgfault_upcall`. 

> **Exercise 8.** Implement the `sys_env_set_pgfault_upcall` system call. Be sure to enable permission checking when looking up the environment ID of the target environment, since this is a "dangerous" system call.

```diff
$ git diff
diff --git a/kern/syscall.c b/kern/syscall.c
index 81d673f..1e0b129 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -144,7 +144,12 @@ static int
 sys_env_set_pgfault_upcall(envid_t envid, void *func)
 {
        // LAB 4: Your code here.
-       panic("sys_env_set_pgfault_upcall not implemented");
+       struct Env *e;
+       if (envid2env(envid, &e, 1) < 0)
+               return -E_BAD_ENV;
+
+       e->env_pgfault_upcall = func;
+       return 0;
 }
```

### Normal and Exception Stacks in User Environments

At normal execution, the user environment runs on the *normal* user stack, in region `[USTACKTOP-PGSIZE, USTACKTOP]`. When page fault occurs, the kernel would make the user environment run the designated page fault handler on the *user exception stack*. In essence, we will implement automatic "stack switching" for the user environment, in a similar way that the x86 *processor* already implements stack switching when transferring from user mode to kernel mode! Recall the processor uses information in TSS to do this stack switching.

Each user environment that wants to support user-level page fault handling needs to allocate memory for its own exception stack, using the `sys_page_alloc()` system call in part A. The exception stack was allocated in `lib/pgfault.c:set_pgfault_handler`. If another page fault happens when executing the page fault handler, `tf->tf_esp` points to the exception stack already. 

### Invoking the User Page Fault Handler

We call the state of the user environment at the time of the user-mode page fault the *trap-time* state.

If there is no page fault handler registered, the JOS kernel destroys the user environment. Otherwise, the kernel sets up a trap frame, `struct UTrapframe`, on the exception stack.

```
                    <-- UXSTACKTOP
trap-time esp
trap-time eflags
trap-time eip
trap-time eax       start of struct PushRegs
trap-time ecx
trap-time edx
trap-time ebx
trap-time esp
trap-time ebp
trap-time esi
trap-time edi       end of struct PushRegs
tf_err (error code)
fault_va            <-- %esp when handler is run	
```

The kernel arranges for the user environment to resume running the page fault handler on the exception stack with this stack frame. The `fault_va` is the virtual address that caused the page fault..

If the user environment is *already* running on the user exception stack when an exception occurs, then the page fault handler itself has faulted. You should start the new stack frame under the current `tf->tf_esp` rather than at `UXSTACKTOP`. First push an empty 32-bit word, then a `struct UTrapframe`.

> **Exercise 9.** Implement the code in `page_fault_handler` in `kern/trap.c` required to dispatch page faults to the user-mode handler. Be sure to take appropriate precautions when writing into the exception stack. (What happens if the user environment runs out of space on the exception stack?)

### User-mode Page Fault Entrypoint

> **Exercise 10.** Implement the `_pgfault_upcall` routine in `lib/pfentry.S`. The interesting part is returning to the original point in the user code that caused the page fault. You'll return directly there, without going back through the kernel. The hard part is simultaneously switching stacks and re-loading the EIP.

> **Exercise 11.** Finish `set_pgfault_handler()` in `lib/pgfault.c`.

`pgfault_upall` is interesting. The manipulation of trapframe is similar to HW5.

```diff
$ git diff
diff --git a/kern/syscall.c b/kern/syscall.c
index 81d673f..bfdd069 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -144,7 +144,12 @@ static int
 sys_env_set_pgfault_upcall(envid_t envid, void *func)
 {
        // LAB 4: Your code here.
-       panic("sys_env_set_pgfault_upcall not implemented");
+       struct Env *e;
+       if (envid2env(envid, &e, 1) < 0)
+               return -E_BAD_ENV;
+
+       e->env_pgfault_upcall = func;
+       return 0;
 }
 
 // Allocate a page of memory and map it at 'va' with permission
@@ -195,8 +200,10 @@ sys_page_alloc(envid_t envid, void *va, int perm)
        // If no page mapped originally, page_remove does nothing
        page_remove(e->env_pgdir, va);
 
-       if ((res = page_insert(e->env_pgdir, allocated, va, perm)) < 0)
+       if ((res = page_insert(e->env_pgdir, allocated, va, perm)) < 0) {
+               page_free(allocated);
                return res;
+       }
 
        return 0;
 }
@@ -381,9 +388,15 @@ syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                return sys_exofork();
        case SYS_env_set_status:
                return sys_env_set_status(a1, a2);
+       case SYS_env_set_pgfault_upcall:
+               return sys_env_set_pgfault_upcall(a1, (void *)a2);
        case SYS_yield:
                sys_yield();
                return 0;
+       case SYS_ipc_try_send:
+               panic("Unimplemented!\n");
+       case SYS_ipc_recv:
+               panic("Unimplemented!\n");
        default:
                return -E_INVAL;
        }
@@ -384,6 +381,46 @@ page_fault_handler(struct Trapframe *tf)
 
        // LAB 4: Your code here.
 
+       // Check with handler
+       if (curenv->env_pgfault_upcall == NULL)
+               goto destroy;
+
+       // Check with allocated user stack
+       user_mem_assert(curenv, (void *)(UXSTACKTOP-PGSIZE), PGSIZE, PTE_W);
+
+       // Set up exception stack
+       uint32_t exceptionStackTop = UXSTACKTOP;
+       if (UXSTACKTOP - PGSIZE <= tf->tf_esp && tf->tf_esp <= UXSTACKTOP - 1) {
+               exceptionStackTop = tf->tf_esp;
+       }
+
+       // Reserve blank word
+       exceptionStackTop -= 4;
+
+       // Construt trap frame
+       exceptionStackTop -= sizeof(struct UTrapframe);
+       if (exceptionStackTop < USTACKTOP - PGSIZE)
+               goto destroy;
+
+       struct UTrapframe *utf = (struct UTrapframe *)exceptionStackTop;
+       utf->utf_esp = tf->tf_esp;
+       utf->utf_eflags = tf->tf_eflags;
+       utf->utf_eip = tf->tf_eip;
+       utf->utf_regs = tf->tf_regs;
+       utf->utf_err = tf->tf_err;
+       utf->utf_fault_va = fault_va;
+
+       // Invoke page fault handler.
+       // Similar to HW5, You cannot execute the handler on the kernel stack.
+       // Manipulate `tf` and use env_run to branch to the registered handler.
+       tf->tf_eip = (uint32_t)curenv->env_pgfault_upcall;
+       tf->tf_esp = exceptionStackTop;
+
+       // Branches to handler
+       // curenv->env_pgfault_upcall is pfentry.S:_pgfault_upcall.
+       env_run(curenv);
+
+destroy:
        // Destroy the environment that caused the fault.
        cprintf("[%08x] user fault va %08x ip %08x\n",
                curenv->env_id, fault_va, tf->tf_eip);
diff --git a/lib/pfentry.S b/lib/pfentry.S
index 819f3aa..7e49a7e 100644
--- a/lib/pfentry.S
+++ b/lib/pfentry.S
@@ -66,17 +66,38 @@ _pgfault_upcall:
        #
        # LAB 4: Your code here.
 
+       # This code executes on the exception stack.
+       # For non-recursive case, trap-time stack is the user stack.
+       # For recursive case, trap-time stack is the exception stack.
+       # utf->utf_esp points to the trap-time stack.
+       # urf->utf_esp is 12 words, 0x30 bytes, above current %esp.
+       # urf->utf_eip is 10 words, 0x28 bytes, above current %esp
+       movl 0x30(%esp), %eax # %eax = *(%esp + 48) = utf->utf_esp
+       movl 0x28(%esp), %ecx # %ecx = *(%esp + 40) = utf->utf_eip
+       subl $4, %eax         # Push utf->utf_eip
+       movl %ecx, (%eax)
+
+       # As we have pushed onto the trap-time stack, we should update %esp
+       # to switch to.
+       movl %eax, 0x30(%esp)
+
        # Restore the trap-time registers.  After you do this, you
        # can no longer modify any general-purpose registers.
        # LAB 4: Your code here.
+       addl $8, %esp   # Discard fault_va, tf_err
+       popal
 
        # Restore eflags from the stack.  After you do this, you can
        # no longer use arithmetic operations or anything else that
        # modifies eflags.
        # LAB 4: Your code here.
+       addl $4, %esp   # Discard utf_eip
+       popf
 
        # Switch back to the adjusted trap-time stack.
        # LAB 4: Your code here.
+       popl %esp
 
        # Return to re-execute the instruction that faulted.
        # LAB 4: Your code here.
+       ret
\ No newline at end of file
diff --git a/lib/pgfault.c b/lib/pgfault.c
index a975518..5c57a89 100644
--- a/lib/pgfault.c
+++ b/lib/pgfault.c
@@ -29,7 +29,14 @@ set_pgfault_handler(void (*handler)(struct UTrapframe *utf))
        if (_pgfault_handler == 0) {
                // First time through!
                // LAB 4: Your code here.
-               panic("set_pgfault_handler not implemented");
+
+               // We're in user-mode, so use provided syscalls
+               int res;
+               if ((res = sys_page_alloc(0, (void *)(UXSTACKTOP-PGSIZE), PTE_W|PTE_U|PTE_P)) < 0)
+                       panic("sys_page_alloc %e\n", res);
+
+               if ((res = sys_env_set_pgfault_upcall(0, _pgfault_upcall)) < 0)
+                       panic("sys_env_set_pgfault_upcall %e\n", res);
        }

```



## Implementing Copy-on-Write Fork

Explains the logic flow in `fork()` and `pgfault()`.

`fork()` create a new environment, then scan through the parent environment's entire address space and set up corresponding page mappings in the child. The key difference is that, while `dumbfork()` copied pages, `fork()` will initially only copy page mappings. 

The basic control flow for fork() is as follows:

- The parent installs `pgfault()` as the C-level page fault handler, using `set_pgfault_handler()`.
- The parent calls `sys_exofork()` to create a child environment.
- For each writable or copy-on-write page in its address space below `UTOP`, the parent calls `duppage`, which should map the page copy-on-write into the address space of the child and then remap the page copy-on-write in its own address space. `duppage` sets both PTEs so that the page is not writeable, and to contain `PTE_COW` in the "avail" field to distinguish copy-on-write pages from genuine read-only pages.
- The exception stack is not remapped this way, however. Instead you need to allocate a fresh page in the child for the exception stack. Since the page fault handler will be doing the actual copying and the page fault handler runs on the exception stack, the exception stack cannot be made copy-on-write.
- The parent sets the user page fault entrypoint for the child to look like its own.
The child is now ready to run, so the parent marks it runnable.

Control flow for the user page fault handler:  
- The kernel propagates the page fault to `_pgfault_upcall`, which calls `fork()`'s `pgfault()` handler.
- pgfault() checks that the fault is a write (check for FEC_WR in the error code) and that the PTE for the page is marked PTE_COW. If not, panic.
- pgfault() allocates a new page mapped at a temporary location and copies the contents of the faulting page into it. Then the fault handler maps the new page at the appropriate address with read/write permissions, in place of the old read-only mapping.
- The user-level `lib/fork.c` code must consult the environment's page tables for several of the operations above (e.g., that the PTE for a page is marked PTE_COW). The kernel maps the environment's page tables at UVPT exactly for this purpose. It uses a clever mapping trick to make it to make it easy to lookup PTEs for user code. `lib/entry.S` sets up `uvpt` and `uvpd` so that you can easily lookup page-table information in lib/fork.c.

> **Exercise 12.** Implement `fork`, `duppage` and `pgfault` in `lib/fork.c`.

```diff
$ git diff
diff --git a/kern/trap.c b/kern/trap.c
index f7639e8..9b717f6 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -385,9 +385,6 @@ page_fault_handler(struct Trapframe *tf)
        if (curenv->env_pgfault_upcall == NULL)
                goto destroy;
 
-       // Check with allocated user stack
-       user_mem_assert(curenv, (void *)(UXSTACKTOP-PGSIZE), PGSIZE, PTE_W);
-
        // Set up exception stack
        uint32_t exceptionStackTop = UXSTACKTOP;
        if (UXSTACKTOP - PGSIZE <= tf->tf_esp && tf->tf_esp <= UXSTACKTOP - 1) {
@@ -397,6 +394,10 @@ page_fault_handler(struct Trapframe *tf)
        // Reserve blank word
        exceptionStackTop -= 4;
 
+       // Check with allocated user stack
+       uint32_t sz = sizeof(struct Trapframe);
+       user_mem_assert(curenv, (void *)(exceptionStackTop - sz), sz, PTE_W);
+
        // Construt trap frame
        exceptionStackTop -= sizeof(struct UTrapframe);
        if (exceptionStackTop < USTACKTOP - PGSIZE)
diff --git a/lib/fork.c b/lib/fork.c
index 61264da..e5365df 100644
--- a/lib/fork.c
+++ b/lib/fork.c
@@ -6,18 +6,31 @@
 // PTE_COW marks copy-on-write page table entries.
 // It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
 #define PTE_COW                0x800
+#define NO_PTE         0xFFFFFFFF
+
+extern volatile pte_t uvpt[];     // VA of "virtual page table"
+extern volatile pde_t uvpd[];     // VA of current page directory
+
+pte_t get_pte(void *va) {
+       // Needs to check if the page table exists, using upvd
+       return uvpd[PDX(va)] & PTE_P ? uvpt[PGNUM(va)] : NO_PTE;
+}
 
 //
 // Custom page fault handler - if faulting page is copy-on-write,
 // map in our own private writable copy.
 //
+// YY: This handler runs on the exception stack.
 static void
 pgfault(struct UTrapframe *utf)
 {
        void *addr = (void *) utf->utf_fault_va;
        uint32_t err = utf->utf_err;
+       pte_t pte = get_pte(addr);
        int r;
 
+       cprintf("pgfault handler invoked in [%x], addr: 0x%08x\n", sys_getenvid(), addr);
+
        // Check that the faulting access was (1) a write, and (2) to a
        // copy-on-write page.  If not, panic.
        // Hint:
@@ -25,6 +38,10 @@ pgfault(struct UTrapframe *utf)
        //   (see <inc/memlayout.h>).
 
        // LAB 4: Your code here.
+       bool isWrite = utf->utf_err & FEC_WR;
+       bool isCOW = pte & PTE_COW;
+       if (!isWrite || !isCOW)
+               panic("Addr: %08x, isWrite: %d, isCOW: %d\n", addr, isWrite, isCOW);
 
        // Allocate a new page, map it at a temporary location (PFTEMP),
        // copy the data from the old page to the new page, then move the new
@@ -34,7 +51,10 @@ pgfault(struct UTrapframe *utf)
 
        // LAB 4: Your code here.
 
-       panic("pgfault not implemented");
+       void *remap_va = (void *)ROUNDDOWN((uint32_t)addr, PGSIZE);
+       sys_page_alloc(0, (void *)PFTEMP, PTE_W|PTE_U|PTE_P);
+       memcpy(PFTEMP, remap_va, PGSIZE);
+       sys_page_map(0, PFTEMP, 0, remap_va, PTE_U|PTE_P|PTE_W);
 }
 
 //
@@ -48,13 +68,34 @@ pgfault(struct UTrapframe *utf)
 // Returns: 0 on success, < 0 on error.
 // It is also OK to panic on error.
 //
+// YY: If the virtual page is not present in our space, silently does nothing.
 static int
 duppage(envid_t envid, unsigned pn)
 {
        int r;
 
        // LAB 4: Your code here.
-       panic("duppage not implemented");
+
+       // We're in user-mode, but we need to access the page tables.
+       // Use the ones mapped at UVPT.
+       void *va;
+       pte_t pte;
+
+       va = (void *) (pn * PGSIZE);
+       pte = get_pte(va);
+       if (pte == NO_PTE || (pte & PTE_P) == 0)
+               return 0;
+
+       if ((pte & PTE_W) || (pte & PTE_COW)) {
+               if ((r = sys_page_map(0, va, envid, va, PTE_U|PTE_COW|PTE_P)) < 0)
+                       panic("duppage: cannot map COW into new env. %e\n", r);
+
+               if ((r = sys_page_map(0, va, 0, va, PTE_U|PTE_COW|PTE_P)) < 0)
+                       panic("duppage: cannot remap COW back, %e\n");
+       } else {
+               if ((r = sys_page_map(0, va, envid, va, PTE_U|PTE_P)) < 0)
+                       panic("duppage: cannot map read-only into new env, %e\n", r);
+       }
        return 0;
 }
 
@@ -78,7 +119,42 @@ envid_t
 fork(void)
 {
        // LAB 4: Your code here.
-       panic("fork not implemented");
+       envid_t child_eid;
+       uint32_t va;
+
+       // cprintf("fork called in [%x]\n", sys_getenvid());
+       // sys_exofork() calls env_alloc() to allocate a new environment.
+       // env_alloc() calls env_setup_vm() to set up the kernel portion of
+       // the new env's address space (everything above UTOP).
+       // sys_exofork() copies the register set, and returns 0 in the child env.
+       set_pgfault_handler(pgfault);
+       child_eid = sys_exofork(); // 0 in child environment
+
+       if (child_eid > 0) {
+               // Duplicate user portion
+               for (va = 0; va < UXSTACKTOP - PGSIZE; va += PGSIZE) {
+                       duppage(child_eid, va / PGSIZE);
+               }
+
+               // New page is allocated for exception stack.
+               sys_page_alloc(child_eid, (void *)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W);
+
+               // Set page fault entrypoint for child
+               sys_env_set_pgfault_upcall(child_eid, thisenv->env_pgfault_upcall);
+
+               // set child status
+               sys_env_set_status(child_eid, ENV_RUNNABLE);
+       } else {
+               // set thisenv
+               for (int i = 0; i < 1024; ++i) {
+                       if (envs[i].env_id == sys_getenvid()) {
+                               thisenv = &envs[i];
+                               break;
+                       }
+               }
+       }
+
+       return child_eid;
 }
```

Not too many lines of code. But it took me a bit long to get it right. Points worthy of notice:

1. Use UVPT to access the page directory and page tables. For detail, refers to `uvpt.md`. Remember to ensure that the page table exists before accessing PTE.
2. When inputting address as argument to functions, make sure you're using the right address.
   - In `pgfault`, when doing `memcpy` and `sys_page_map`, you should use `remap_va`, instead of `addr`. 
   - In `fork`, allocate exception stack at `(UXSTACKTOP - PGSIZE)`, instead of `UXSTACKTOP`. 
3. In `duppage`, when mapping COW pages, remember to map them as NOT writtable. 
4. Fixed the bug in Lab3 in `trap.c:page_fault_handler`. More precise check using `user_mem_assert`. 
5. The return value of `sys_exofork` is different in child and parent.

**Question**: Why we need to mark pages in both child process and parent process as COW? What if we did not mark the pages in the parent process as COW?

Answer: If pages are not marked as COW in the parent process, and the child doesn't write to that page. Then the parent process could write to its page and the changes would be seen by the child process. However, `fork` wants the chlid process to have separate address space.

**Question**: In `duppage`, we map the page copy-on-write into the address space of the child and then *remap* the page copy-on-write in its own address space. Why must we do it in this order?

Answer: Suppose we first mark pages in parent as COW. Before we mark pages in child as COW, the parent process writes to the page (image this page is the user stack) and thus a new page would be allocated, and this would be mapped to the child process and marked as COW. Up to now, everything is good. But consider the previous question: suppose later the parent process writes to the new page again, this change would be experienced by the child process! Again, the child doesn't have a separate address space.

