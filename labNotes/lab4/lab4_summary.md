# Lab4 Summary

The multiprocessor support is not an important topic. Pay more attention to the implementations of naive `fork()`, Copy-On-Write `fork()`, preemptive multitasking, and IPC. 

## Part A: Multiprocessor Support and Cooperative Multitasking

### Multiprocessor Support [x]

Symmetric Multi-Processing (SMP) model, bootstrap processor (BSP), application processor (AP). LAPIC, Memory-mapped I/O (MMIO).

Explains the control flow of JOS when booting multiple processors.

Explains data structure in JOS that models per-CPU state: kernel stack, TSS, `curenv`, register set.

Explaines the locking scheme in JOS.

### Round-Robin Scheduling

Implements round-robin scheduling.

### System Calls for Environment Creation

Implement system calls for implementing **user-space** fork, in `user/dumbfork.c:dumbfork`. 



## Part B: Copy-on-Write Fork

Explains the main mechanism of copy-on-write. 

### User-level page fault handling

It's common to set up different handlers for different kinds of page faults. The handler should take different actions based on different faulting addresses. 

Explains how the exception stack is used in page fault handling. After the page fault handler finishes, the control is directly transferred back to the user code, instead of through the kernel code. 

### Implementing Copy-on-Write Fork

UVPT technique.

Implements copy-on-write fork.


Entire process of `fork()` and COW: 
- `fork()` does the following: 
    - installs `pgfault()` as page fault handler; 
    - creates a new environment and copies all page mapping from parent into child, marking all writable pages as read-only and COW in both parent and child; 
    - allocates exception stack for child; 
    - install the same page fault handler in child; 
    - mark child as runnable.
- In `trap.c` and `trapentry.S`, an entry for page fault is installed into interrupt vector table.
- When an environment tries to write to a COW page, page fault happens, the normal interrupt mechanism plays: 
    - hardware and software constructs a `struct Trapframe` on kernel stack and calls `trap`;
    - `trap` dispatches to `page_fault_handler`;
    - `page_fault_handler` constructs `struct UTrapframe` on exception stack, and transfers control to `pfentrys.S:_pgfault_upcall`;
    - `_pgfault_upcall` calls `_pgfault_handler`, `pgfault()`, which verifies the faulting address and replaces the COW page by a writable copy of the faulted page. 
    - `_pgfault_upcall` continues execution and returns directly to the user instruction that causes the page fault.


## Part C: Preemptive Multitasking and Inter-Process communication (IPC)

Up to this point, we only have preemptive multitasking. The round-robin scheduling we implemented in Part B only functions when the user process voluntarily releases the CPU.



### Clock Interrupts and Preemption

Explains `FL_IF` flag and the interrupt schemem in JOS. Implements hardware interrupt handling scheme.

Implements preemptive multitasking, essentially timer interrupt handling.

### Inter-Process communication (IPC)

Implements IPC.