# Lec10

## Discussion on HW7

- What does `idelock` protect?

  `idequeue`.

- What goes wrong with adding `sti`/`cli` in `iderw`?

  When an IDE interrupt goes off, trap would dispatch to `ideintr`, the interrupt handler. `ideintr` would try to acquire the `idelock`.
  Suppose in `iderw` we re-enable interrupts after acquired the `idelock`, and the IDE interrupt goes off before we release the lock, `ideintr` tries to acquire the `idelock`, but finds out that the CPU is already holding the same lock(`holding()` returns true if the cpu is already holding the lock), and would panic.

- What would happen if `acquire` does not do `holding()` and `panic()`?

  While `iderw` is holding the `idelock`, IDE interrupt goes off, `ideintr` takes over and tries to acquire the `idelock`. But the lock is already held by `iderw` and thus the interrupt handler waits. Note that as `idelock` is a spin-lock, this execution just hangs!

- What happens to the interrupt in the original code? This means if we disable interrupt when acquiring the lock, and checks for reacquiring the same lock on the same CPU.

  A little knowledge of how CPU detects interupts. There's a interrupt line the CPU that gets written to whenever an interrupt goes off. Before each instruction fetch/decode, CPU would lookup the interrupt and see if there's any interrupt that needs to be served. Disabling interrupt with `cli()` means the step of checking interrupt line is not performed, but the interrupt line is still written to when any interrupt goes off.

  In the original `iderw`, we turn off interrupt, acquires lock, does whatever we need to do, release the lock and re-enables interrupt. Suppose during the time being, an IDE interrupt goes off. Then after enabling the interrupt, the CPU would look up the interrupt line and see it. We would not lose the interrupt.

  One thing you should keep in mind is that: you can only retrieve the latest unserved interrupt, but you have no information about previous ones (if any) as it just gets overwritten. This is consistent with what you leared in CSAPP.

- What if IDE interrupt had occured on a different core?

  CPU 0 turns off interrupt on its own CPU, acquries `idelock` in `iderw`. Before CPU 0 releases the lock, an IDE interrupt goes off in CPU 1, calls `acquire`, `holding()` evaluates to true, and spin-waits for CPU 0 to releases the lock, which would eventually happens. This is the behavior we want.

- Why `acquire` disables interrupts **before** waiting for the lock?

  Suppose `acquire` disables interrupts after waiting. There's a period when lock is acquired but interrupt is enabled. Suppose interrupt happens at this point and `ideintr` runs and `acquire` is called again. Since lock is already held in `iderw`, deadlock happen!



## Process Scheduling

- Goals

  - Transparent to user process
  - Preemptive for user process
  - Preemptive for kernel, to improve responsive

- xv6 solution

  - 1 user thread and 1 kernel thread per process
  - 1 scheduler per CPU

- xv6 context switch scheme

  - User thread -> kernel thread, through system call or interrupt
  - kernel thread -> scheduler thread, by cooperative yielding, `yield`
  - scheduler thread finds a RUNNABLE thread
  - scheduler thread -> kernel thread
  - kernel thread -> user thread

  This is explained in detail in the xv6 book. You might refer to `xv6-book-notes/5 - scheduling.md`. Note that this also shows the two kinds of register saving: (1) when trapping into kernel, (2) when switching between processes. 



## Code

1. Why the context does not include registers like eax, ecx, edx?

   `swtch` would only be called in kernel code in a cooperative approach, in `yield()` or `scheduler()`, and all kernel code are following the gcc calling convention: caller-saved registers can be manipulated in any way, so they do not need to be saved.

2. Why `swtch` does not save eip?

   %eip would be automatically pushed onto the kernel stack when the kernel thread makes a fucntion call. This is, again, part of the gcc calling convention.

3. How `swtch` works?

   When executing on the original kernel stack, registers are saved to construct a context. Then the stack is switched. On the new stack, the context of its caller was saved previously. Thus, restore the context. Note that %eip is implicitly restored with `ret`: it pops the top of the stack into eip.

4. `swtch` returns to another kernel thread, not to the original one!



## Thread Cleanup

`kill` marks the thread `p->killed = 1`, and `trap()` would calls `exit()`. `Exit()` would wake up any waiting parent, pass abandoned child to `init`, mark the current thread as ZOMBIE and calls `sched`, which switches to the scheduler. The scheduler would never tries to run this thread because its state is ZOMBIE.

But where does the killed process's resources get freeed? Only in `wait()`. If the parent does not wait, then those ZOMBIE threads would be pass again to `init` and gets waited by `init`.