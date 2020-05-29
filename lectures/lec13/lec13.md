# Lec13

## Logging in xv6

Logging = Journaling.

The way xv6 uses logging is called "write-ahead log": Install ***none*** of a transaction's writes to disk until ***all*** writes are in the log on disk ***and*** the logged writes are marked committed.



### Challenge: prevent write-back from cache

A system call can safely update a cached block, but the modified block cannot be written to the file system until the transaction completes. 

Suppose all slots cache modified blocks, how to make space for new block to be cached? Solution: (1) big cache space; (2) mark dirtry pages in the cache and unmark after the transaction.



## Xv6 log representation

Refer to `log.c`.

```c
begin_op();
...
bp = bread(...);
bp->data[...] = ...;
log_write(bp);
...
end_op();
```

After modifying `bp->data`, `log_write` record the block number of the modified block in the log header and pin in the cache with `B_DIRTY`. The modified data is still in the cache.

`End_op` tries to commit the change by calling `commit()`. 

```c
static void commit() {
  // When commit() is called, all existing logs at the time
  // is grouped into one transaction.
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}
```

`log.lh.n` indicates whether there's transaction pending to be written to the disk. If `log.lh.n`, the log is valid and is a valid transaction. 



### Challenge: Syscall's data must fit in log

Compute an upper bound of number of blocks each system call writes, and set log size >= upper bound. Then break up system calls into several transactions. However, breaking up syscalls, like large `write`, into transactions makes it non-atomic. The crash would leave a correct prefix of the write.

In `begin_op`, xv6 allows no new syscalls to start if the data might not fit in the log. 



#### Challenge: a block may be written many times in one transaction

A cached block may reflect multiple uncommitted transactions. It's wrong if they get written to the disk when committing. Xv6 prevents this from happening by checking `log.outstanding` in `end_op` and ensures that installed blocks only reflect committed transactions.

```
$ echo a > x
// create
bwrite 3   inode, 35					Writing log
bwrite 4   directory content, 63		Writing log
bwrite 2   commit (block #s and n)		Commit
bwrite 35  install inode				Perform write described by log
bwrite 63  install directory content	Perform write described by log
bwrite 2   mark log "empty"				Mark transaction done

// write
bwrite 3	Bitmap update				Writing log		
bwrite 4	data block update, 'a'		Writing log					
bwrite 5	inode update				Writing log
bwrite 2								Commit
bwrite 58   bitmap						Perform write described by log
bwrite 533  x							Perform write described by log
bwrite 35   inode (file size)			Perform write described by log
bwrite 2								Mark transaction done

// write
bwrite 3	data block content			Writing log		
bwrite 4	inode update				Writing log
bwrite 2								Commit
bwrite 533  \n							Perform write described by log
bwrite 35   inode						Perform write described by log
bwrite 2								Mark transaction done
```

- what is good about xv6's log design?
  - correctness due to write-ahead log
  - good disk throughput: log naturally batches writes
  - concurrency

- what's wrong with xv6's logging?
  - not very efficient:
        every block is written twice (log and install)
        logs whole blocks even if only a few bytes modified
        writes each log block synchronously
          could write them as a batch and only write head synchronously 
        log writes and install writes are eager
          both could be lazy, for more write absorbtion
          but must still write the log first
  - trouble with operations that don't fit in the log
        unlink might dirty many blocks while truncating file