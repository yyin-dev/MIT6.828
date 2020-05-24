# Chapter 5 - Scheduling

## Multiplexing

Xv6 switches processes on the processor in two cases. First, xv6's `sleep` and `wakeup` switches when a process waits (for I/O, for a child to exit, or sleeps). Second, xv6 periodically forces a switch when a proces is executing user instructions. 

The challenges are: (1) how to switch? (2) How to switch transparently? (3) How to avoid race conditions? (4) A process's resources should be freed when it exits, then it cannot do this by itself since, for example, it cannot free its own kernel stack. (5) For multi-core machines, each core must remember which process it was executing so that system calls affect the correct process's kernel stack.



## Code: Context switching

![](contextSwitch.jpg)

Steps involved in a switch from one user process to another:

- Trap (syscall/interrupt) into old process's kernel thread
- context switch to current CPU's scheduler thread
- context switch to the new process's kernel thread
- trap return to the new process's user thread

Xv6 needs scheduler thread since it's sometimes unsafe to execute on any process's kernel stack. Let's example switch between kernel thread and the scheduler thread.

Switching from one thread to another involves saving current thread's CPU registers, and restoring the new thread's CPU registers. Saving and restoring %esp and %eip causes change of stack and the executed code.

`swtch.S:swtch` performs the saves and restores. When it's time for a process to give up the CPU, the processor's kernel thread calls `swtch` to save its context, `struct context*`,  and return to the scheduler thread. `swtch` takes two arguments: `struct context **old` and `struct context *new`. It pushes current registers onto the stack and saves the stack pointer in `*old`. The `swtch` copies `new` into %esp, pops saved registers, and returns.

Consider `yield`. `yield` calls `sched`, which calls `swtch(&p->context, mycpu()->scheduler);` to switch to the scheduler's thread.

```assembly
.globl swtch
swtch:
  # Copy args into %eax, %edx
  movl 4(%esp), %eax # p->context
  movl 8(%esp), %edx # &(c->scheduler)

  # Save old callee-saved registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  movl %esp, (%eax) # *(p->context) = %esp
  movl %edx, %esp   # %esp = &(c->scheduler)

  # Load new callee-saved registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret # Return to forkret
```

`swtch` first copies arguments from stack, before changing the stack pointer. The it pushes a `struct context` onto the current stack. Only the callee-saved registers are saved: %ebp, %ebx, %esi, %edi, %esp. The first four are saved explicitly. %esp is saved implicity by writing it to `*old`. Also,, %eip has been pushed onto the stack earlier, by the `call` instruction that invoked `swtch`. Now all registers are saved.

Then, `swtch` moves the pointer to the new context into %esp, and restores saved registers. In our example  `swtch(&p->context, mycpu()->scheduler);`, `swtch` switches to `cpu->scheduler`, the per-CPU scheduler context. That context was saved previously by the scheduler's `swtch`. 

Note that this `swtch` call in `sched` returns not to `sched`, but to `scheduler`. 



## Code: Scheduling

Given `swtch`, consider how to switch from one process to another. A process that wants to give up the CPU must acquire `ptable.lock`, release any other locks, update its own state `proc->state`, and calls `sched`. `Sched` knows a lock is held when it's called, so it knows interrupts are disabled. `Sched` calls `swtch` to save the curent context in `proc->context` and swtch to the scheduler's context. `swtch` returns on the scheduler's stack as though `scheduler`'s `swtch` has returned. The scheduler continues the for loop, finds another process's to run, switches to it, and the cycle repeats.

We saw that xv6 holds `ptable.lock` across calls to `swtch`. So `ptable.lock` is acquired in one thread but released in another. This's necessary for contexting swtiching to protect the invariants on the process's `state` and `context` that're not true while executing `swtch`.

A kernel thread always gives up the processor in `sched` and switches to some fixed location in the scheduler thread, and then switches to some kernel thread that previously called `sched`. 

`scheduler` holds `ptable.lock` most of the time, but releases the lock and explicitly enables interrupts once in each iteration of the outer loop. This is important for the special case when the CPU is idle and there's no `RUNNABLE` process. If an idling scheduler looped with the lock continuously held, no other non-idle CPU could perform a context switch (as that requires acquiring the lock). The reason to enable interrupt periodically on the idling CPU is that there might be no `RUNNABLE` processes because all processes are waiting for I/O. If the scheduler never enables interrupt, the I/O never arrives.

Once scheduler finds a `RUNNABLE` process, it sets per-CPU variable `proc`, switches page table with `switchuvm`, marks the process as `RUNNING`, and calls `swtch`. 

**Reflection**: One way to think about the structure of the scheduling code, and particular holding `ptable.lock` across threads, is to enforce certain invariants about each process, and holding `ptable.lock` whenever those invariants aren't true. One invariant is that when a process is `RUNNING`, the CPU registers should hold the process's register values, %cr3 must hold the process's page table, %esp must refer to the process's kernel stack. Maintaing the invariants is the reason why xv6 holds lock across different threads. 



## Code: mycpu and myproc

Requires disabling interrupts. Read the book for explanation.