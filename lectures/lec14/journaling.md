# Jouraling the Linux ext2fs Filesystem

## Goal

- Performance shouldn't suffer too much.
- Compatibility with existing applications shouldn't break.
- The reliability of the file system shouldn't be compromised.

## Reliability

- Preservation: on-disk data stable before crash should be preserved.
- Predictability: the failure modes from which we have to recover should be predictable.
- Atomicity: each operation is atomic.

## Existing implementations

Non-journaling filesystems require scanning the entire disk to achieve predictability. So performance is sacrificed.

Journaling filesystems makes transactions atomic and doesn't have to scan the entire disk.

## Designing a new filesystem for Linux

We want a journaling FS, as we want short recovery time. Journaling FS only needs to scan the journals for recover, as the possibly inconsistent data must be in the journal.

Another advantage of journaling scheme is that journals are stored separately from the permanent data. So the original on-disk structure of existing code can be reused.

Thus, we're not designing a new FS for Linux. We are adding a new feature, transactional FS journaling, to existing ext2fs.

## Anatomy of a transaction

Disk writes caused by one operation should be contained in one single transaction, to ensure consistency.

Transactions could read from existing contents of the FS, and that imposes an ordering between transactions. A transaction which modifes a block on disk cannot commit after a transactionwhich reads that new data.

Before we can commit a transaction which allocates new blocks to a file, we have to make sure that all data blocks being created by the transaction have been written to disk.

## Merging transactions

Rather than create a separate transaction for each system call, we simply create a new transaction periodically, and allow all FS service calls to add their updates to that single system-wide compound transaction. This improves performance.

## On-disk representation

Ext2fs reserves a number of inode numbers. We use one of the reserved inode numbers to store the FS journal. So the journal IS a file.

## Format of the filesystem journal

The journal file's job: it records the new contents of metadata blocks while we're in the process of committing transactions. The requirement is that we must be able to atomically commit the transactions the journal contains.

We write three types of data blocks to the journal: metadata, descriptor and header blocks.

A journal metadata block contains the contents of a block of FS metadata that's updated by a transaction. Thus, however small a change we make to the metadata, we write an entire journal block to log the change.

Descriptor blocks are journal blocks that describe other journal metadata blocks. Whenever we want to write metadata blocks to the journal, we need to record which disk blocks the metadata normally lives at, so that the recovery mechanism can copy the metadata back into the main FS.

Both descriptor and metadata blocks are written sequentially to the journal. At all times, we maintain the current head of the log (the block number of the last block written) and the tail (the oldest block in the log that hasn't been pinned). Whenever we run out of log space, we stall new log writes until the tail has been cleaned up to free more space. 

Header blocks are at fixed location in the journal file. These records the current head and tail of the journal, plus a sequence number. At recovery time, the header blocks are scanned to find the header block with the highest sequence number.

## Committing and checkingpointing the journal

At some point, either because we haven't commit for long, or because we are running out of journal space, we wish to commit the outstanding journal as a transaction.

Once the transaction has been committed, we need to keep track of the metadata buffers recorded in the transation so that we can notice when the get written back to the main location on disk.

When commit a transaction, the updated blocks are in the journal, but not their permanent location. To completely commit and finish checkpointing a transaction, we do:

1. Close the transaction. Future FS operations would be in a new transaction to be created.
2. Start flushing the transaction to the disk. This means writing metadata buffers to the journal. When a buffer is committed, mark it until it's no longer dirty.
3. Wait for all outstanding FS operations in this transaction to complete.
4. Wait for all outstanding transaction updates to be recorded in the journal.
5. Update the journal header blocks to record the new head and new tail, committing the transaction to disk.
6. When we wrote the transaction's updated buffers to the journal, we marked them. These buffers are unmarked only when they have been copied from the journal to their permanent on-disk location. Only when the transaction's last buffer becomes unmarked can we reuse the journal blocks occupied by the transaction. 



## Collisions between transactions

To increase performance, we don't completely suspend FS updates while commiting a transaction. Rather, we create a new transaction to record updates that arrive while we're commiting the old transaction. 

What to do if an update wants to access a metadata buffer owned by another, older transaction which is currently being committed? 

If the new transaction only wants to read the buffer, then no problem. A dependency is created but since transactions are committed in sequential order, no problem.

If the new transaction wants to write to the buffer. we make a new copy of the buffer. One copy is given to the new transaction for modification, while the other is left owned by the old transaction. 





> Mid-way down the left column on page 6, the Journaling paper says "However, until we have finished syncing those buffers, we cannot delete the copy of the data in the journal." Give a concrete example in which removing this rule would lead to disaster.

Suppose each block of the journal data is deleted right after that block has been copied to the right location on the disk. If the crash after the copying begins (so some blocks have been copied and deleted) but before all copying is done, then we cannot recover.