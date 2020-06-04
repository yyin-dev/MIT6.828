# Lec 14 Linux ext3 crash recovery

## Xv6 logging

- Logging for crash recovery

  - xv6: slow but immediately durable
  - ext3: fast but not immediately durable

  Trade-off: safety vs performance.

- Xv6 logging

  In-memory buffer --> On-disk log --> On-disk Filesystem.

  Each system call, involving a `end_op()`, is a transaction.

  At the end of each system call:

  - `log_write` updates in-memory log header.
  - `write_log` writes buffer to on-disk log.
  - `write_header` writes in-memory log header to disk. This is the commit point.
  - `install_trans` copies logs to FS.
  - Updata in-memory header and `write_header` updates on-disk log header.

- Problem with xv6 logging

  Slow. Recall that in HW11, `cat a` after recovery prints empty file.

  Immediate commit after each system call, and immediate write to FS after each commit. All syscalls block during execution of `commit()` and the logging scheme writes every block twice to the disk: one to log, one to FS.

  So the writes are synchronous. For example, creating an empty file takes 6 synchronous disk writes: 2 * 3 = 6, 3 = bitmap + inode block + data block of directory. 

  

## Linux ext3 design

Ext3 adds logging to ext2, a log-less FS.

- ext3 structures
  - In memory
    - Write-back block cache
    - Per-transaction informatio: 
      - set of block numbers to be logged
      - set of outstanding handles - one per syscall
  - On disk:
    - FS
    - Log
- What's in ext3 on-disk log?
  - Log superblock: log offset and starting sequence number of earliest valid transaction
  - Descriptor blocks: magic number, sequence number, block numbers
  - Data blocks
  - Commit blocks: magic number, sequence number.

- How does ext3 get good performance?

  - Batching: commites every few seconds, instead of after each syscall
  - Syscall returns before the data is written to disk
  - Concurrent transactions: some fully committed in the on-disk log, some doing the log writes to disk, one open transaction accepting new syscalls.

- ext3 syscall code

  ```
  sys_open() {
  	h = start()
  	get(h, block_no)
  	modify the block in the cache
  	stop(h)
  }
  ```

  - `start()`: tells logging system to make writes atomic until `stop()`. The logging system must know the set of outstanding system calls and cannot commit until they're complete. `Start()` can block the syscall if needed.

  - `get()`: tells logging system that we'll modify cached block, add it to list of blocks to be logged. Pin the block in memory until the transaction commits.

  - `stop()`: transaction can commit if and only if all included syscalls have called `stop()`.

- Committing a transaction to disk
    1. block new syscalls
    2. wait for in-progress syscalls to stop()
    3. open a new transaction, unblock new syscalls
    4. write descriptor to log on disk with list of block #s
    5. write each block from cache to log on disk
    6. wait for all log writes to finish (step 5)
    7. write the commit record
    8. wait for the commit write to finish (step 6)
    9. now cached blocks allowed to go to homes on disk (but not forced)

- Can syscall B read uncommitted results of syscall A?

  A: `rm x`

  B: `echo > y`, reusing x's freed i-node

  Could B commit first and cause problem?

  - Case 1: both in same transaction. Ok, both or neither. 

  - Case 2: A in T1, B in T2. Ok, ext3 commits transactions in order.

  - Case 3: B in T1, A in T2.

    ```
    in T1: |--B--|
    in T2:    |--A--|
    ```

    Could B see A's free of i-node? If yes and the crash happens after T1 finishes but before T2 finishes, both `x` and `y` using the same i-node after reboot.

    This's impossible since ext3 waits for all syscalls in prev transaction to finish before letting in any new syscalls in next transaction. Thus, B (in T1) completes before ext3 lets A (in T2) start, so B won't see A's writes.

    So:

    ```
    T1: |-syscalls-|
    T2:            |-syscalls-|
    T3:                       |-syscalls-|
    ```

    Note that this doesn't mean each transaction only contains one syscall. Blocking of new syscalls only happen when we try to commit.

  The point: the commit order must be consistent with the order from which the syscalls read/write state. If the commit order is consistent with the syscall order, then we have correctness. Perhaps ext3 sacrifices some performace to gain correctness.

- Is it safe for a syscall in T2 to write a block that was also written in T1?

  Ext3 allows T2 to start before T1 finishes committing, for better performance.

  ```
  T1: |-syscalls-|-commitWrites-|
  T2:            |-syscalls-|-commitWrites-|
  ```

  The dangerous situation: T1 syscall writes block 17. Then T1 closes, starts writing cached block to log. If T1 writes T2's modified block 17 to T1 transaction in the log, bad things happen.

  So ext3 gives T1 a private copy of the block cache as it existed when T1 closed. This can be efficient using copy-on-write. The copies allow syscalls in T2 to proceed while T1 is committing. 

- When can ext3 re-use transaction T1's log space?

  If all transactions prior to T1 have been freed in the log and T1's cached blocks have all been written to log on disk.

  Free: advance log superblock's start pointer/seq.

- What if not enough free space in log for a syscall?

  Suppose mid way through committing a transaction, we realize T2 won't fit onto logs.

  We cannot commit T2, since not all syscalls are logged. We cannot back off since there's no way to undo a syscalls.

  Solution: reservation. Syscall pre-declares the number of blocks of log space it might need. Ext3's `start()` blocks until there's enough space in the on-disk log. 

- Crash recovery

  1. Find he start of the log -- the first non-freed descriptor. The log superblock contains offset and sequence number of the first transaction.

  2. Find the end of the log

     Scan until bad magic or unexpected sequence number. Go back to last commit record.

  3. Replay all blocks through last complete transaction, in log order.

  

## Summary

- The classic write-ahead logging rule
  - Don't write metadata block to on-disk FS until commtted in on-disk log
  - Wait for all syscalls in T1 to finish before starting T2 (syscalls block in `start()`)
  - Don't overwrite a block in buffer cache before it's in the log (separate copy)
  - Don't free log space until all blocks have been written to FS
- Ext3's fix to xv6 performance issue
  - Synchronous write to on-disk log -- yes, but of certain window
  - Every tiny update causes block write -- maybe, if write absorbtion supported
  - Synchronous writes to how location after commit -- yes