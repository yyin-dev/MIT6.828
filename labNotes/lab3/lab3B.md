# Lab3 Part B: Page Faults, Breakpoints Exceptions and System Calls

> **Exercise 5.** Modify `trap_dispatch()` to dispatch page fault exceptions to `page_fault_handler()`. You should now be able to get make grade to succeed on the `faultread`, `faultreadkernel`, `faultwrite`, and `faultwritekernel` tests. If any of them don't work, figure out why and fix them. 

```diff
$ git diff
diff --git a/kern/trap.c b/kern/trap.c
index 3cd38a9..d04884d 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -187,6 +187,13 @@ trap_dispatch(struct Trapframe *tf)
 {
        // Handle processor exceptions.
        // LAB 3: Your code here.
+       switch (tf->tf_trapno) {
+       case T_PGFLT:
+               page_fault_handler(tf);
+               break;
+       default:
+               break;
+       }
 
        // Unexpected trap: The user process or the kernel has a bug.
        print_trapframe(tf);

```

> **Exercise 6.** Modify `trap_dispatch()` to make breakpoint exceptions invoke the kernel monitor. You should now be able to get make grade to succeed on the `breakpoint` test.

```diff
$ git diff
diff --git a/kern/trap.c b/kern/trap.c
index d04884d..ab5116b 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -93,7 +93,7 @@ trap_init(void)
        SETGATE(idt[T_DIVIDE], 0, GD_KT, DIVIDE, 0);
        SETGATE(idt[T_DEBUG], 1, GD_KT, DEBUG, 0);
        SETGATE(idt[T_NMI], 0, GD_KT, NMI, 0);
-       SETGATE(idt[T_BRKPT], 1, GD_KT, BRKPT, 0);
+       SETGATE(idt[T_BRKPT], 1, GD_KT, BRKPT, 3); // Lab3 Exercise5: allow user's call
        SETGATE(idt[T_OFLOW], 1, GD_KT, OFLOW, 0);
        SETGATE(idt[T_BOUND], 0, GD_KT, BOUND, 0);
        SETGATE(idt[T_ILLOP], 0, GD_KT, ILLOP, 0);
@@ -187,10 +187,15 @@ trap_dispatch(struct Trapframe *tf)
 {
        // Handle processor exceptions.
        // LAB 3: Your code here.
+       cprintf("trap_dispathch: tf->trapno = %u\n", tf->tf_trapno);
+
        switch (tf->tf_trapno) {
        case T_PGFLT:
                page_fault_handler(tf);
                break;
+       case T_BRKPT:
+               monitor(tf);
+               break;
        default:
                break;
        }
```

Note that you need to set `T_BRKPT` as callable by user-mode.

> **Questions**
>
> 3. The break point test case will either generate a break point exception or a general protection fault depending on how you initialized the break point entry in the IDT (i.e., your call to `SETGATE` from `trap_init`). Why? How do you need to set it up in order to get the breakpoint exception to work as specified above and what incorrect setup would cause it to trigger a general protection fault?
>
> 4. What do you think is the point of these mechanisms, particularly in light of what the `user/softint` test program does?

3. If `T_BRKPT` is not allowed to be called in user-mode, then protection fault is generated. 
4. The OS controls what service it provides, and protects the kernel.



> **Exercise 7.** Add a handler in the kernel for interrupt vector `T_SYSCALL`. You will have to edit `kern/trapentry.S` and `kern/trap.c`'s `trap_init()`. You also need to change `trap_dispatch()` to handle the system call interrupt by calling `syscall()` (defined in `kern/syscall.c`) with the appropriate arguments, and then arranging for the return value to be passed back to the user process in `%eax`. Finally, you need to implement `syscall()` in `kern/syscall.c`. Make sure `syscall()` returns `-E_INVAL` if the system call number is invalid. You should read and understand `lib/syscall.c` (especially the inline assembly routine) in order to confirm your understanding of the system call interface. Handle all the system calls listed in `inc/syscall.h` by invoking the corresponding kernel function for each call.

```diff
$ git diff
diff --git a/kern/syscall.c b/kern/syscall.c
index 414d489..4831c82 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -21,6 +21,20 @@ sys_cputs(const char *s, size_t len)
        // Destroy the environment if not.
 
        // LAB 3: Your code here.
+       uint32_t s_start = (uint32_t) s;
+       uint32_t s_end = (uint32_t)s + len;
+       uint32_t start_va = ROUNDDOWN(s_start, PGSIZE);
+       uint32_t end_va = ROUNDUP(s_end, PGSIZE);
+
+       uint32_t va;
+       for (va = start_va; va < end_va; va += PGSIZE) {
+               // Use curenv->env_pgdir instead of kern_pgdir
+               pte_t* pte = pgdir_walk(curenv->env_pgdir, (void *)va, 0);
+               if (pte == NULL || (*pte & PTE_U) == 0) {
+                       cprintf("kern/syscall.c:sys_cputs: Memory error, destory env\n");
+                       env_destroy(curenv);
+               }
+       }
 
        // Print the string supplied by the user.
        cprintf("%.*s", len, s);
@@ -70,9 +84,25 @@ syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
        // Return any appropriate return value.
        // LAB 3: Your code here.
 
-       panic("syscall not implemented");
+       cprintf("kern/syscall.c:syscall: syscallno = %d\n", syscallno);
 
        switch (syscallno) {
+       case SYS_cputs:
+               // cprintf("kern/syscall.c:syscall: %.*s\n\n", (char *)a1, a2);
+               sys_cputs((char *)a1, a2);
+               return 0;
+       case SYS_cgetc:
+               ;
+               int c = sys_cgetc();
+               return c;
+       case SYS_getenvid:
+               ;
+               int envid = sys_getenvid();
+               return envid;
+       case SYS_env_destroy:
+               sys_env_destroy(a1);
+               return 0;
+       case NSYSCALLS:
        default:
                return -E_INVAL;
        }
diff --git a/kern/trap.c b/kern/trap.c
index ab5116b..8a26d46 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -44,6 +44,7 @@ void FPERR();
 void ALIGN();
 void MCHK();
 void SIMDERR();
+void SYSCALL();
 
 static const char *trapname(int trapno)
 {
@@ -88,6 +89,11 @@ trap_init(void)
        // Define default gate for reserved/unused vector
        for (int i = 0; i < 32; ++i) SETGATE(idt[i], 0, GD_KT, 0, 0);
 
+       // Note:
+       // When you want an interrupt/exception to be used by the user,
+       // set the last argument ot SETGATE to 3, not 1!
+       // This seems trivial but can cause subtle bug.
+
        // Refer to IA32-3A 5.3.1 Table 5-1 for isTrap
        // Trap: 1, 3, 4
        SETGATE(idt[T_DIVIDE], 0, GD_KT, DIVIDE, 0);
@@ -109,6 +115,9 @@ trap_init(void)
        SETGATE(idt[T_MCHK], 0, GD_KT, MCHK, 0);
        SETGATE(idt[T_SIMDERR], 0, GD_KT, SIMDERR, 0);
 
+       // From Interl 80386 Manual 9.9, we know syscall is trap
+       SETGATE(idt[T_SYSCALL], 1, GD_KT, SYSCALL, 3);
+
        // Per-CPU setup 
        trap_init_percpu();
 }
@@ -192,12 +201,15 @@ trap_dispatch(struct Trapframe *tf)
        switch (tf->tf_trapno) {
        case T_PGFLT:
                page_fault_handler(tf);
-               break;
+               return;
        case T_BRKPT:
                monitor(tf);
-               break;
-       default:
-               break;
+               return;
+       case T_SYSCALL:
+               // kern/syscall.c:syscall
+               tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax, 
+                                      tf->tf_regs.reg_edx, 
+                                      tf->tf_regs.reg_ecx, 
+                                      tf->tf_regs.reg_ebx, 
+                                      tf->tf_regs.reg_edi, 
+                                      tf->tf_regs.reg_esi);
+               return;

        }
 
        // Unexpected trap: The user process or the kernel has a bug.
diff --git a/kern/trapentry.S b/kern/trapentry.S
index 07e1480..9d47da6 100644
--- a/kern/trapentry.S
+++ b/kern/trapentry.S
@@ -74,6 +74,10 @@ TRAPHANDLER(ALIGN, T_ALIGN)
 TRAPHANDLER_NOEC(MCHK, T_MCHK)
 TRAPHANDLER_NOEC(SIMDERR, T_SIMDERR)
 
+# 48, syscall
+# From Intel 80386 Manual 9.10, we know syscall doesn't push error code
+TRAPHANDLER_NOEC(SYSCALL, T_SYSCALL)
+
```

The system call interface works as follows. `inc/syscall.c` defines the syscall interfaces exposed to users in `inc/lib.h`. When the user invokes a system call, `lib/syscall.c:syscall`'s inline assembly uses `int $0x30` to generate a syscall interrupt, which would be handled by `trap`. `kern/trap.c:trap` dispatches interrupt with `trapno = T_SYSCALL`  to `kern/syscall.c:syscall`, which distributes to actual implementations of the system calls. 

This's not a not a difficult exercise, but took me longer than expected. Several subtle points:

1. When adding a handler to the IDT in `kern/syscall.c:trap_init`, if you want the user to be able to raise the interrupt, you should set the last argument to `SETGATE` to be 3, instead of 1.
2. In `kern/syscall.c:sys_cputs`, use `curenv->env_pgdir` instead of `kern_pgdir`.
3. In `kern/trap.c:trap`, if successfully handled, the function should return in the switch branch, instead of only `break`.

> **Exercise 8.** Add the required code to the user library, then boot your kernel. You should see `user/hello` print "`hello, world`" and then print "`i am environment 00001000`". `user/hello` then attempts to "exit" by calling `sys_env_destroy()` (see `lib/libmain.c` and `lib/exit.c`). Since the kernel currently only supports one user environment, it should report that it has destroyed the only environment and then drop into the kernel monitor.

```diff
$ git diff
diff --git a/lib/libmain.c b/lib/libmain.c
index 8a14b29..fc42204 100644
--- a/lib/libmain.c
+++ b/lib/libmain.c
@@ -13,7 +13,13 @@ libmain(int argc, char **argv)
 {
        // set thisenv to point at our Env structure in envs[].
        // LAB 3: Your code here.
-       thisenv = 0;
+       envid_t eid = sys_getenvid();
+       for (int i = 0; i < 1024; ++i) {
+               if (envs[i].env_id == eid) {
+                       thisenv = &envs[0];
+                       break;
+               }
+       }
 
        // save the name of the program so that panic() can use it
        if (argc > 0)
```

> **Exercise 9.** Change `kern/trap.c` to panic if a page fault happens in kernel mode.
>
> Hint: to determine whether a fault happened in user mode or in kernel mode, check the low bits of the `tf_cs`.
>
> Read `user_mem_assert` in `kern/pmap.c` and implement `user_mem_check` in that same file.
>
> Change `kern/syscall.c` to sanity check arguments to system calls.
>
> Boot your kernel, running `user/buggyhello`. The environment should be destroyed, and the kernel should *not* panic. You should see:
>
> ```
> 	[00001000] user_mem_check assertion failure for va 00000001
> 	[00001000] free env 00001000
> 	Destroyed the only environment - nothing more to do!
> ```

> **Exercise 10.** Boot your kernel, running `user/evilhello`. The environment should be destroyed, and the kernel should not panic. You should see:
>
> ```
> 	[00000000] new env 00001000
> 	...
> 	[00001000] user_mem_check assertion failure for va f010000c
> 	[00001000] free env 00001000
> 	
> ```

```diff
$ git diff
diff --git a/kern/kdebug.c b/kern/kdebug.c
index 283adb7..24bfcfe 100644
--- a/kern/kdebug.c
+++ b/kern/kdebug.c
@@ -142,6 +142,8 @@ debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
                // Make sure this memory is valid.
                // Return -1 if it is not.  Hint: Call user_mem_check.
                // LAB 3: Your code here.
+               if (user_mem_check(curenv, usd, sizeof(struct UserStabData), PTE_U) < 0)
+                       return -1;
 
                stabs = usd->stabs;
                stab_end = usd->stab_end;
@@ -150,6 +152,10 @@ debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info)
 
                // Make sure the STABS and string table memory is valid.
                // LAB 3: Your code here.
+               if (user_mem_check(curenv, stabs, (stab_end - stabs) * sizeof(struct Stab), PTE_U) < 0)
+                       return -1;
+               if (user_mem_check(curenv, stabstr, stabstr_end - stabstr_end, PTE_U) < 0)
+                       return -1;
        }
 
        // String table validity checks
diff --git a/kern/pmap.c b/kern/pmap.c
index bbaf570..abb5cea 100644
--- a/kern/pmap.c
+++ b/kern/pmap.c
@@ -590,6 +590,27 @@ int
 user_mem_check(struct Env *env, const void *va, size_t len, int perm)
 {
        // LAB 3: Your code here.
+       uint32_t va_start = ROUNDDOWN((uint32_t) va, PGSIZE);
+       uint32_t va_end = ROUNDUP((uint32_t)va + len, PGSIZE);
+       uint32_t vaddr;
+
+       if (va_start >= ULIM) {
+               user_mem_check_addr = MAX(va_start, (uint32_t)va);
+               return -E_FAULT;
+       }
+       if (va_end >= ULIM) {
+               user_mem_check_addr = ULIM;
+               return -E_FAULT;
+       }
+
+       int required_perm = perm | PTE_P;
+       for (vaddr = va_start; vaddr < va_end; vaddr += PGSIZE) {
+               pte_t *pte = pgdir_walk(env->env_pgdir, (void *)vaddr, 0);
+               if (pte == NULL || (*pte & required_perm) != required_perm) {
+                       user_mem_check_addr = MAX((uint32_t)va, vaddr);
+                       return -E_FAULT;
+               }
+       }
 
        return 0;
 }
diff --git a/kern/syscall.c b/kern/syscall.c
index 4831c82..d4395bd 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -21,20 +21,7 @@ sys_cputs(const char *s, size_t len)
        // Destroy the environment if not.
 
        // LAB 3: Your code here.
-       uint32_t s_start = (uint32_t) s;
-       uint32_t s_end = (uint32_t)s + len;
-       uint32_t start_va = ROUNDDOWN(s_start, PGSIZE);
-       uint32_t end_va = ROUNDUP(s_end, PGSIZE);
-
-       uint32_t va;
-       for (va = start_va; va < end_va; va += PGSIZE) {
-               // Use curenv->env_pgdir instead of kern_pgdir
-               pte_t* pte = pgdir_walk(curenv->env_pgdir, (void *)va, 0);
-               if (pte == NULL || (*pte & PTE_U) == 0) {
-                       cprintf("kern/syscall.c:sys_cputs: Memory error, destory env\n");
-                       env_destroy(curenv);
-               }
-       }
+       user_mem_assert(curenv, (void *)s, len, PTE_U);
 
        // Print the string supplied by the user.
        cprintf("%.*s", len, s);
diff --git a/kern/trap.c b/kern/trap.c
index 45f08e8..1dddab6 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -276,6 +274,9 @@ page_fault_handler(struct Trapframe *tf)
        // Handle kernel-mode page faults.
 
        // LAB 3: Your code here.
+       if ((tf->tf_cs & 3) == 0) {
+               panic("kernel page fault!\n");
+       }
 
        // We've already handled kernel-mode exceptions, so if we get here,
        // the page fault happened in user mode.
```

The major part of this exercise is `user_mem_check`. Be careful about the logic.