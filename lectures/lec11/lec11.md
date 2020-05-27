# Lec11

## Homework: uthread_switch.S

- Why does the code copy `next_thread` to `current_thread`?

  `Uthread_switch.S` switches from `current_thread` to `next_thread`. `Current_thread` points to the thread currently running. So `next_thread` should be assigned to `current_thread`. 

- Why it's ok for `thread_yield` to call scheduler, but not kernel's scheduler?

  This's all in user space. The user threads live within the user space of one process, and `thread_yield` can be considered as intra-process scheduling. When OS wants to switch from this process to another, the process's kernel thread executes and calls kernel's scheduler.

- What happens when a uthread blocks in a syscall?

  When one uthread blocks in syscall, the entire process is descheduled. In other words, no other uthreads in the same process would execute. 

- Do our uthreads take advantage of multi-core for parallel execution?

  No. All uthreads are within the same process and the scheduler cannot schedule one process on multiple cores.



## Sequence Coordination

Sleep and wait is needed. Spinlock is inefficient.

Be aware of possible spurious wakeups. The wakeup is only a hint that something happened, but no guarantee that the condition is true. So use a while loop for `sleep`.

Two problems needs to be solved: (1) lost wakeup; (2) temrination while sleeping.

### Lost wakeup

Lock is needed to prevent lost wakeups. But that can cause deadlock. (Explained in xv6 book).

Xv6's strategy: Both the lock on the condition as well as `ptable.lock` are held when `wakeup` looks for sleepers. `Sleep` requires acquiring `ptable.lock`. So only two possibilities: 

- `Sleep` finishes before `wakeup` is called. No problem.
- `Wakeup` is called before the potential sleeper checks the condition. The potential sleeper wouldn't call `sleep`.



### Terminate a sleeping thread

`kill` doesn't forcibly terminate the process, as it might not be safe. So it just sets `p->killed` and wakes up the process if it's sleeping. Later, `trap` would call `exit`. In other words, if the target is sleeping, we want it to stop sleeping and call `exit`. However, what if the process is in some operation that should be atomic, like creating a file?

Solution: Some sleep loops check for `p->killed`, so the response to `kill` is fast. Other loops doesn't check for `p->killed`, to ensure that current operation (that may requires to be atomic) is correctly finished and no inconsistency is caused.