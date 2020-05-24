# HW7

## Don't do this

> Make sure you understand what would happen if the xv6 kernel executed the following code snippet:
>
> ```
> struct spinlock lk;
> initlock(&lk, "test lock");
> acquire(&lk);
> acquire(&lk);
> ```
>
> Explain in one sentence what happens.

- The first and an important problem is where to put the code. `mpmain` is the last function in `main()` in `main.c`, put the code above into the function.

  ```c
  // Common CPU setup code.
  static void mpmain(void) {
      cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
      idtinit();       // load idt register
      xchg(&(mycpu()->started), 1); // tell startothers() we're up
   
      // Add code here
      struct spinlock lk;
      initlock(&lk, "test lock");
      acquire(&lk);
      acquire(&lk);
  
      scheduler();     // start running processes
  }
  ```

  New output:

  ```
  ...
  cpu1: starting 1
  lapicid 1: panic: acquire
  80104431 80102e9a 80102eba 705a 0 0 0 0 0 0
  ```

  Clearly, `holding(lk)` evaluates to true in `acquire` and the kernel panics.



## Interrupts in ide.c

> **Submit**: Explain in a few sentences why the kernel panicked. You may find it useful to look up the stack trace (the sequence of `%eip` values printed by `panic`) in the `kernel.asm` listing.

```
lapicid 1: panic: acquire
 80104431 80102093 801059d5 801056ec 80100183 801019da 80106b26 80100b2d 801053d0 801048a9
```

Tracing:

```
80104431: panic
80102093: next instruction after acquire() in ideintr()
801059d5: call ideintr()
801056ec: next instruction after `trap` returns in trapasm.S
```

After `iderw` acquires lock, `sti()` turns on interrupt. Then interrupt happens and `trap` dispatches to `ideintr` handler. It also tries to call `acquire` on the same lock. And xv6 panics as `if(holding(lk))` evaluates to true.



## Interrupts in file.c

In `file.c`, the for loop in `filealloc` is very quick and short, so it's unlikely that an interrupt happens in the meantime. However, for `ide.c`, disk read/write can be slow. So the time when holding the lock with interrupt enabled is longer, and the chance of panic is higher.



## xv6 lock implementation

Suppose `lk->pcs[0]` and `lk->cpu` is cleared after clearing `lk->locked`. Then there would be a period of time when `lk->locked` is cleared, but `lk->pcs[0]` and `lk->cpu` aren't cleared yet. Suppose another cpu acquires the lock. But then the clearing of `lk->pcs[0]` and `lk->cpu` happens. This is clearly undesired behavior.