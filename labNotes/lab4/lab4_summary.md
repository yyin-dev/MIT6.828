# Lab4 Summary

## Part A: Multiprocessor Support and Cooperative Multitasking

### Multiprocessor Support

Symmetric Multi-Processing (SMP) model, bootstrap processor (BSP), application processor (AP). LAPIC, Memory-mapped I/O (MMIO).

Explains the control flow of JOS when booting multiple processors.

Explains data structure in JOS that models per-CPU state: kernel stack, TSS, `curenv`, register set.

Explaines the locking scheme in JOS.

### Round-Robin Scheduling

Implements round-robin scheduling.

### System Calls for Environment Creation

Implement system calls for implementing **user-space** `fork`. 



## Part B: Copy-on-Write Fork

Explains how copy-on-write works.

### User-level page fault handling

It's common to set up different handlers for differnet kinds of page faults.

Explains how the exception stack is used in page fault handling.

### Implementing Copy-on-Write Fork

UVPT technique.

Implements copy-on-write fork.



## Part C: Preemptive Multitasking and Inter-Process communication (IPC)

Up to this point, we only have preemptive multitasking. The round-robin scheduling we implemented in Part B only functions when the user process voluntarily releases the CPU.



### Clock Interrupts and Preemption

Explains `FL_IF` flag and the interrupt schemem in JOS. Implements hardware interrupt handling scheme.

Implements clock interrupt handling.

Implements IPC.