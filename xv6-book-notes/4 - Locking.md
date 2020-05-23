# Chapter 4 - Locking

Concurrency can exists on uniprocessor system: the atomicity is not guaranteed.

### Race conditions

Lock protects invariants about the data.

### Code: Locks

Spin-lock is implemented using `xchg`  and `movl` instructions to guarantee atomicity in xv6.

### Code: Using locks

Lock granularity.

### Deadlock and lock ordering

Locking ordering prevents deadlock.

### Interrupt handlers

Interrupt can cause concurrency on a single processor, as it breaks the atomicity. If interrupt happens when holding the lock, deadlock can happen.

To avoid this situation, if a spin-lock is used by an interrupt handler, a processor must never hold that lock with interrupts enabled. Actually, xv6 always disable interrupts on a processor when it enters a spin-lock critical section. Interrupts may happen on other processors. 

Xv6 re-enables interrupts when the processor releases the spin-lock. Due to the possibility of nested cirtical section, some book keeping is needed with `pushcli` and `popcli`. 

### Instruction and memory ordering

Compilers might change code ordering. Xv6 uses `__sync_synchronize()` in `acquire` and `release` to tell the compiler not to do so.

### Sleep locks

If a lock needs to held for a long time, spin-lock is inefficient. Efficiency demands that the processor be yielded while waiting so that other threads can make progress. This also means that the lock should work when held across context switches. Sleep-lock.

Used in file systems. Detail skipped for now. 