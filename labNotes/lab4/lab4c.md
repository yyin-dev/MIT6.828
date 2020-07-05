# Lab4C - Preemptive Multitasking and Inter-Process communication (IPC)

> **Exercise 13.** Modify `kern/trapentry.S` and `kern/trap.c` to initialize the appropriate entries in the IDT and provide handlers for IRQs 0 through 15. Then modify the code in `env_alloc()` in `kern/env.c` to ensure that user environments are always run with interrupts enabled.
>
> Also uncomment the `sti` instruction in `sched_halt()` so that idle CPUs unmask interrupts

> **Exercise 14.** Modify the kernel's `trap_dispatch()` function so that it calls `sched_yield()` to find and run a different environment whenever a clock interrupt takes place.

```diff
$ git diff
diff --git a/kern/env.c b/kern/env.c
index d5d94b4..dc6b98e 100644
--- a/kern/env.c
+++ b/kern/env.c
@@ -261,6 +261,7 @@ env_alloc(struct Env **newenv_store, envid_t parent_id)
 
        // Enable interrupts while in user mode.
        // LAB 4: Your code here.
+       e->env_tf.tf_eflags |= FL_IF;
 
        // Clear the page fault handler until user installs one.
        e->env_pgfault_upcall = 0;
diff --git a/kern/sched.c b/kern/sched.c
index fd38132..d3da581 100644
--- a/kern/sched.c
+++ b/kern/sched.c
@@ -92,7 +92,7 @@ sched_halt(void)
                "pushl $0\n"
                "pushl $0\n"
                // Uncomment the following line after completing exercise 13
-               //"sti\n"
+               "sti\n"
                "1:\n"
                "hlt\n"
                "jmp 1b\n"
diff --git a/kern/trap.c b/kern/trap.c
index 9b717f6..9c850ec 100644
--- a/kern/trap.c
+++ b/kern/trap.c
@@ -51,6 +51,12 @@ void MCHK();
 void SIMDERR();
 void SYSCALL();
 
+void TIMER();
+void KBD();
+void SERIAL();
+void SPURIOUS();
+void IDE();
+
 static const char *trapname(int trapno)
 {
        static const char * const excnames[] = {
@@ -104,10 +110,10 @@ trap_init(void)
        // Refer to IA32-3A 5.3.1 Table 5-1 for isTrap
        // Trap: 1, 3, 4
        SETGATE(idt[T_DIVIDE], 0, GD_KT, DIVIDE, 0);
-       SETGATE(idt[T_DEBUG], 1, GD_KT, DEBUG, 0);
+       SETGATE(idt[T_DEBUG], 0, GD_KT, DEBUG, 0);
        SETGATE(idt[T_NMI], 0, GD_KT, NMI, 0);
-       SETGATE(idt[T_BRKPT], 1, GD_KT, BRKPT, 3); // Lab3 Exercise5: allow user's call
-       SETGATE(idt[T_OFLOW], 1, GD_KT, OFLOW, 0);
+       SETGATE(idt[T_BRKPT], 0, GD_KT, BRKPT, 3); // Lab3 Exercise5: allow user's call
+       SETGATE(idt[T_OFLOW], 0, GD_KT, OFLOW, 0);
        SETGATE(idt[T_BOUND], 0, GD_KT, BOUND, 0);
        SETGATE(idt[T_ILLOP], 0, GD_KT, ILLOP, 0);
        SETGATE(idt[T_DEVICE], 0, GD_KT, DEVICE, 0);
@@ -122,8 +128,14 @@ trap_init(void)
        SETGATE(idt[T_MCHK], 0, GD_KT, MCHK, 0);
        SETGATE(idt[T_SIMDERR], 0, GD_KT, SIMDERR, 0);
 
+       SETGATE(idt[IRQ_OFFSET+IRQ_TIMER], 0, GD_KT, TIMER, 0);
+       SETGATE(idt[IRQ_OFFSET+IRQ_KBD], 0, GD_KT, KBD, 0);
+       SETGATE(idt[IRQ_OFFSET+IRQ_SERIAL], 0, GD_KT, SERIAL, 0);
+       SETGATE(idt[IRQ_OFFSET+IRQ_SPURIOUS], 0, GD_KT, SPURIOUS, 0);
+       SETGATE(idt[IRQ_OFFSET+IRQ_IDE], 0, GD_KT, IDE, 0);
+
        // From Interl 80386 Manual 9.9, we know syscall is trap
-       SETGATE(idt[T_SYSCALL], 1, GD_KT, SYSCALL, 3);
+       SETGATE(idt[T_SYSCALL], 0, GD_KT, SYSCALL, 3);
 
        // Per-CPU setup 
        trap_init_percpu();
@@ -260,6 +272,12 @@ trap_dispatch(struct Trapframe *tf)
        // Handle clock interrupts. Don't forget to acknowledge the
        // interrupt using lapic_eoi() before calling the scheduler!
        // LAB 4: Your code here.
+       if (tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER) {
+               cprintf("Timer interrupt on irq 0\n");
+               lapic_eoi();
+               sched_yield();
+               return;
+       }
 
        // Unexpected trap: The user process or the kernel has a bug.
        print_trapframe(tf);
diff --git a/kern/trapentry.S b/kern/trapentry.S
index 72cfda8..40ed785 100644
--- a/kern/trapentry.S
+++ b/kern/trapentry.S
@@ -75,6 +75,14 @@ TRAPHANDLER(ALIGN, T_ALIGN)
 TRAPHANDLER_NOEC(MCHK, T_MCHK)
 TRAPHANDLER_NOEC(SIMDERR, T_SIMDERR)
 
+# (+32) 0 - 15 ==> 32 - 47, IRQ (interrupt request)
+# The processor never pushes error code for IRQ
+TRAPHANDLER_NOEC(TIMER, IRQ_OFFSET+IRQ_TIMER)
+TRAPHANDLER_NOEC(KBD, IRQ_OFFSET+IRQ_KBD)
+TRAPHANDLER_NOEC(SERIAL, IRQ_OFFSET+IRQ_SERIAL)
+TRAPHANDLER_NOEC(SPURIOUS, IRQ_OFFSET+IRQ_SPURIOUS)
+TRAPHANDLER_NOEC(IDE, IRQ_OFFSET+IRQ_IDE)
+
```

The exercises are not hard, but two poins worthy of notice:

1. How to disable interrupt when trapping into kernel? Use the `istrap` parameter of `SETGATE`.
2. Remeber to call `lapic_eio` when handling timer interrupt. This's in the comment but not noticable enough.


### IPC in JOS
You will implement two system calls, `sys_ipc_recv` and `sys_ipc_try_send`. Then you will implement two library wrappers `ipc_recv` and `ipc_send`.

The "messages" that user environments can send to each other using JOS's IPC mechanism consist of two components: a single 32-bit value, and optionally a single page mapping. Allowing environments to pass page mappings in messages provides an efficient way to transfer more data than will fit into a single 32-bit integer, and also allows environments to set up shared memory arrangements easily.

### Sending and Receiving Messages
To receive a message, an environment calls `sys_ipc_recv`. This system call de-schedules the current environment and does not run it again until a message has been received. When an environment is waiting to receive a message, any other environment can send it a message - not just a particular environment, and not just environments that have a parent/child arrangement with the receiving environment. 

To try to send a value, an environment calls `sys_ipc_try_send`. If the named environment is actually receiving (it has called `sys_ipc_recv` and not gotten a value yet), then the send delivers the message and returns 0. Otherwise the send returns `-E_IPC_NOT_RECV` to indicate that the target environment is not currently expecting to receive a value.

A library function `ipc_recv` in user space will take care of calling `sys_ipc_recv` and then looking up the information about the received values in the current environment's `struct Env`.

Similarly, a library function `ipc_send` will take care of repeatedly calling `sys_ipc_try_send` until the send succeeds.

### Transferring Pages

When an environment calls `sys_ipc_recv` with a valid `dstva` parameter (below UTOP), the environment is stating that it is willing to receive a page mapping. If the sender sends a page, then that page should be mapped at `dstva` in the receiver's address space. If the receiver already had a page mapped at `dstva`, then that previous page is unmapped.

When an environment calls `sys_ipc_try_send` with a valid `srcva` (below UTOP), it means the sender wants to send the page currently mapped at `srcva` to the receiver, with permissions `perm`. After a successful IPC, the sender keeps its original mapping for the page at `srcva` in its address space, but the receiver also obtains a **mapping** for this same physical page at the `dstva` originally specified by the receiver, in the receiver's address space. As a result this page becomes **shared** between the sender and receiver.

If either the sender or the receiver does not indicate that a page should be transferred, then no page is transferred. After any IPC the kernel sets the new field `env_ipc_perm` in the receiver's Env structure to the permissions of the page received, or zero if no page was received.

> **Exercise 15.** Implement `sys_ipc_recv` and `sys_ipc_try_send` in `kern/syscall.c`. Read the comments on both before implementing them, since they have to work together. When you call `envid2env` in these routines, you should set the `checkperm` flag to 0, meaning that any environment is allowed to send IPC messages to any other environment, and the kernel does no special permission checking other than verifying that the target envid is valid.
>
> Then implement the `ipc_recv` and `ipc_send` functions in `lib/ipc.c`.

```diff
$ git diff
diff --git a/kern/syscall.c b/kern/syscall.c
index bfdd069..27f82c6 100644
--- a/kern/syscall.c
+++ b/kern/syscall.c
@@ -294,6 +294,10 @@ sys_page_unmap(envid_t envid, void *va)
        return 0;
 }
 
+bool isAligned(void *addr) {
+       return (uint32_t)addr % PGSIZE == 0;
+}
+
 // Try to send 'value' to the target env 'envid'.
 // If srcva < UTOP, then also send page currently mapped at 'srcva',
 // so that receiver gets a duplicate mapping of the same page.
@@ -336,7 +340,35 @@ static int
 sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
 {
        // LAB 4: Your code here.
-       panic("sys_ipc_try_send not implemented");
+       struct Env *e;
+       int res;
+       pte_t *pte = NULL;
+
+       if ((res = envid2env(envid, &e, 0)) < 0) return res;
+       if (!e->env_ipc_recving) return -E_IPC_NOT_RECV;
+
+       if ((uint32_t)srcva < UTOP) {
+               pte = pgdir_walk(curenv->env_pgdir, srcva, 0);
+               bool inappropriate = !(perm & PTE_U) || !(perm & PTE_P) || (perm & ~PTE_SYSCALL);
+               if (!isAligned(srcva) || inappropriate || !pte) return -E_INVAL;
+       }
+       if ((perm & PTE_W) && pte && !(*pte & PTE_W)) return -E_INVAL;
+
+       
+       e->env_ipc_recving = 0;
+       e->env_ipc_from = curenv->env_id;
+       e->env_ipc_value = value;
+       if ((uint32_t)srcva < UTOP && (uint32_t)e->env_ipc_dstva < UTOP) {
+               e->env_ipc_perm = 0;
+               struct PageInfo* pp = page_lookup(curenv->env_pgdir, srcva, NULL);
+               if (!pp) return -E_INVAL;
+               if ((res = page_insert(e->env_pgdir, pp, e->env_ipc_dstva, perm)) < 0) return res;
+               e->env_ipc_perm = perm;
+       }
+       e->env_tf.tf_regs.reg_eax = 0;
+       e->env_status = ENV_RUNNABLE;
+
+       return 0;
 }
 
 // Block until a value is ready.  Record that you want to receive
@@ -354,10 +386,17 @@ static int
 sys_ipc_recv(void *dstva)
 {
        // LAB 4: Your code here.
-       panic("sys_ipc_recv not implemented");
-       return 0;
+       if ((uint32_t)dstva < UTOP && !isAligned(dstva)) return -E_INVAL;
+
+       curenv->env_ipc_recving = 1;
+       curenv->env_ipc_dstva = dstva;
+       curenv->env_status = ENV_NOT_RUNNABLE;
+       sched_yield();
+
+       return 0; // Not run
 }
 
+
 // Dispatches to the correct kernel function, passing the arguments.
 int32_t
 syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
@@ -368,7 +407,6 @@ syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
 
        switch (syscallno) {
        case SYS_cputs:
-               // cprintf("kern/syscall.c:syscall: %.*s\n\n", (char *)a1, a2);
                sys_cputs((char *)a1, a2);
                return 0;
        case SYS_cgetc:
@@ -394,9 +432,9 @@ syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                sys_yield();
                return 0;
        case SYS_ipc_try_send:
-               panic("Unimplemented!\n");
+               return sys_ipc_try_send(a1, a2, (void *)a3, a4);
        case SYS_ipc_recv:
-               panic("Unimplemented!\n");
+               return sys_ipc_recv((void *)a1);
        default:
                return -E_INVAL;
        }
diff --git a/lib/ipc.c b/lib/ipc.c
index 2e222b9..133f295 100644
--- a/lib/ipc.c
+++ b/lib/ipc.c
@@ -23,8 +23,17 @@ int32_t
 ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
 {
        // LAB 4: Your code here.
-       panic("ipc_recv not implemented");
-       return 0;
+       int res;
+
+       if ((res = sys_ipc_recv(pg ? pg : (void*)UTOP)) < 0) {
+               if (from_env_store) *from_env_store = 0;
+               if (perm_store) *perm_store = 0;
+               return res;
+       }
+
+       if (from_env_store) *from_env_store = thisenv->env_ipc_from;
+       if (perm_store) *perm_store = thisenv->env_ipc_perm;
+       return thisenv->env_ipc_value;
 }
 
 // Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
@@ -39,7 +48,14 @@ void
 ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
 {
        // LAB 4: Your code here.
-       panic("ipc_send not implemented");
+       int res;
+       while (true) {
+               res = pg ? sys_ipc_try_send(to_env, val, pg, perm) 
+                                : sys_ipc_try_send(to_env, val, (void*)UTOP, 0);
+               if (res == 0) return;
+               if (res != -E_IPC_NOT_RECV) panic("Unexpected error: %e\n", res);
+               sys_yield();
+       }
 }
```

This exercise is not hard and the comments give detailed guide. But a previous bug in `sched_yield` took me several hours to locate (and I still don't fully understand the reason). Old code:

```c
void
sched_yield(void)
{
	static int prev_run = -1;
	struct Env *e;

	for (int i = (prev_run + 1) % NENV; i != prev_run; i = (i + 1) % NENV) {
		e = &envs[i];
		if (e->env_status == ENV_RUNNABLE) {
			prev_run = i;
			env_run(e);
		}
	}
    
	// No other RUNNABLE env, except the previously run
	if (prev_run >= 0 && envs[prev_run].env_status == ENV_RUNNING) {
		e = &envs[prev_run];
		env_run(e);
	}

	// sched_halt never returns
	sched_halt();
}
```

Code that works:

```c
void
sched_yield(void)
{
	// LAB 4: Your code here.
	struct Env *idle = curenv;
	int idle_envid = (idle == NULL) ? -1 : ENVX(idle->env_id);
	int i;

	// search envs after idle
	for (i = idle_envid + 1; i < NENV; i++) {
		if (envs[i].env_status == ENV_RUNNABLE) env_run(&envs[i]);
	}

	// find from 1st env if not found
	for (i = 0; i < idle_envid; i++) {
		if (envs[i].env_status == ENV_RUNNABLE) env_run(&envs[i]);
	}

	// if still not found, try idle
	if(idle != NULL && idle->env_status == ENV_RUNNING) {
		env_run(idle);
	}

	// sched_halt never returns
	sched_halt();
}
```

