# Lab5

## File system preliminaries

We'll develop a FS much simpler than most real FSes, including xv6, but supporting creating, writing, and deleting files organized in a hierarchical directory structure. Also, we only support single-user.

### On-disk File System Structure

Most Unix FS divide disk space into two main parts: *inode* region and *data* region. Unix FS assigns one inode to each file. The inode holds metadata about the file, while the data region stores file data and directory meta-data. Directory entries contain file names and pointers to inodes. A file is *hard-linked* if multiple directory entries refer to that file's inode. Our FS wouldn't support hard links, so we don't need this level of indirection: we won't use inodes at all and store all of a file's metadata within the directory entry describing that file.

Files and directories consists of a series of data blocks, which can be scattered throughout the disk. Our FS allows user to *read* directory metadata, enabling user to implement directory scanning operations like `ls` themselves. The disadvantage of this approach is that it makes application programs dependent on the format of the directory meta-data. 

### Soft link and hard link

Difference between *symbolic link* and *hard link*: hard link directly points to the inode maintained by the OS, deleting one hard link doesn't affect other links accessing the file; a symbolic link (aka soft link) is a pointer to another link. If the earlier link is deleted, the symbolic link wouldn't work.  

Soft link example:
```console
$ echo "This is the source file." > source.txt
$ cat source.txt
This is the source file.
$ ln -s source.txt softLink.txt
$ cat source.txt
This is the source file.
$ cat softLink.txt
This is the source file.
$ ls -lis
total 0
 9288674231454612 0 lrwxrwxrwx 1 yy0125 yy0125 10 Jul 13 06:44 softLink.txt -> source.txt
16325548649219578 0 -rwxrwxrwx 1 yy0125 yy0125 25 Jul 13 06:44 source.txt
$ rm source.txt
$ cat softLink.txt
cat: softLink.txt: No such file or directory
```
Observation: `source.txt` and `softLink.txt` displays the same data. However, they have different inode number and `softLink.txt` points to `source.txt`: `softLink.txt -> source.txt`. If `source.txt` is deleted, `softLink.txt` is no longer available. Soft link is just a shortcut pointing to the original file.  

Note that the size of `source.txt` is 25 bytes (the length of "This is the source file.\0"), while the size of `softLink.txt` is 10 bytes (the length of "source.txt")! 
```
   inode ["This is the source file."]   inode["source.txt"]
            ^                               ^
            |                               |
        source.txt                      softLink.txt
```

Hard link example:
```console
$ echo "This is the source file." > source.txt
$ cat source.txt
This is the source file.
$ ln source.txt hardLink.txt
$ cat source.txt
This is the source file.
$ cat hardLink.txt
This is the source file.
$ ls -lis
total 0
16325548649219578 0 -rwxrwxrwx 2 yy0125 yy0125 25 Jul 13 06:44 hardLink.txt
16325548649219578 0 -rwxrwxrwx 2 yy0125 yy0125 25 Jul 13 06:44 source.txt
$ echo "Changed." > source.txt
$ cat source.txt
Changed.
$ cat hardLink.txt
Changed.
$ rm source.txt
$ cat hardLink.txt
Changed.
```
Note that `source.txt` and `hardLink.txt` has the same inode number, and they are exactly the same! Changes in one file is seen by the other, and removing one hard link doesn't affect the other hard links.
```
    inode ["This is the source file."]   
            ^           ^                       
            |           |                    
        source.txt   hardLink.txt
```


### Sectors and Blocks

Disks perform read/write at *sector* granularity. In JOS, each sector is 512 bytes. FS uses disk storage at *block* granularity. So sector size is a hardware property, while block size is a software quantity. A FS's block size must be a multiple of the underlying disk. Our FS use block size of 4096 bytes, matching the processor's page size.

### Superblocks

<img src="https://pdos.csail.mit.edu/6.828/2018/labs/lab5/disk.png" alt="Disk layout" style="zoom:80%;" />

Metadata for the FS itself is stored at *superblocks*. Our FS has exactly one superblock, at block 1. Defined by `struct Super`.

The picture above is specific to Lab5 FS, as it doesn't have inode regions. A more general organization of on-disk structures of FS:
```
+-------------+------------+--------------+-------------+--------------+-------------+
| boot sector | superblock | inode bitmap | data bitmap | inode region | data region |
+-------------+------------+--------------+-------------+--------------+-------------+
```

### File Meta-data

![FileStructure](FileStructure.png)

Defined in `struct File`. As we don't have inodes, the metadata is stored in a directory entry on disk. Unlike in real FSes, we use this to represent file metadata as it appears both on disk and in memory.

### Directory versus Regular Files

A `File` struture can represent either a *regular file* or a directory, dintinguished by the `type` field. The superblock contains a `File` structure holding the meta-data for the root directory. 



## The File System

The goal for the lab is not to implement the entire FS, but only certain key components. We'll implement:

- reading blocks into block cache and flusing them back to disk
- allocating disk blocks
- mapping file offsets to disk blocks
- read, write, open in the IPC interface

But it's imporant to be familar with the provided code and other interfaces.



## Disk Access

We'll not add an IDE disk driver to the kernel with necessary system calls, but implement the IDE disk driver as part of the user-level file system envionment. This is easy if we use polling, "programmed I/O" (PIO)-based access and don't use disk interrupts. Interrupt-driven device drivers in user mode is possible but harder. 

The x86 processor uses the IOPL bits in the EFLAGS register to determine whether 32-bit protected-mode code is allowed to perform special device I/O instructions such as the IN and OUT instructions. So giving I/O privilege to the FS environment is the only thing needed to allow the FS to access the registers. In effect, the IOPL bits in the EFLAGS provides the kernel with a simple "all-or-nothing" method of controlling whether user-mode code can access I/O space. In our case, we want the file system environment to be able to access I/O space, but not any other environments.

> **Exercise 1.** `i386_init` identifies the file system environment by passing the type `ENV_TYPE_FS` to your environment creation function, `env_create`. Modify `env_create` in `env.c`, so that it gives the file system environment I/O privilege, but never gives that privilege to any other environment.

```diff
$ git diff
diff --git a/kern/env.c b/kern/env.c
index 7656e16..8bf9712 100644
--- a/kern/env.c
+++ b/kern/env.c
@@ -426,6 +426,10 @@ env_create(uint8_t *binary, enum EnvType type)
                panic("env_create: env_alloc fails\n");
        }
 
+       if (type == ENV_TYPE_FS) {
+               env->env_tf.tf_eflags |= FL_IOPL_MASK;
+       }
+
```

> **Question**
>
> 1. Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

No. The registers are saved and restored automatically by process switching code. And no other code in JOS changes the value of IOPL. So it's enough to set value at this stage.



## The Block Cache

Code in `fs/bc.c`. JOS uses disk of size 3GB or less. We reserve a 3GB region of the file system environment's address space from 0x10000000 (`DISKMAP`) up to 0xD0000000 (`DISKMAP + DISKMAX`), as a "memory-mapped" version of the disk. For example, disk block 0 is mapped at virtual address 0x10000000, disk block 1 is mapped at virtual address 0x10001000, and so on. The `diskaddr` function in `fs/bc.c` implements this translation. We reserve most of the file system environment's address space in this way.

It's slow to read the entire disk into memory. So we do something similar to demand paging, where we only allocate pages in the disk map region and read the correspoinding block from the disk in response to a page fault in the region. 

So different from xv6 where we have special structrures for block cache. In JOS, the cache is the memory space of the FS environment.

> **Exercise 2.** Implement the `bc_pgfault` and `flush_block` functions in `fs/bc.c`.

```diff
$ git diff
diff --git a/fs/bc.c b/fs/bc.c
index e3922c4..6184553 100644
--- a/fs/bc.c
+++ b/fs/bc.c
@@ -49,6 +49,11 @@ bc_pgfault(struct UTrapframe *utf)
        //
        // LAB 5: you code here:
 
+       uint32_t secno = blockno * BLKSIZE / SECTSIZE;
+       addr = ROUNDDOWN(addr, PGSIZE);
+       sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W);
+       ide_read(secno, addr, 1);
+
        // Clear the dirty bit for the disk block page since we just read the
        // block from disk
        if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
@@ -72,12 +77,30 @@ void
 flush_block(void *addr)
 {
        uint32_t blockno = ((uint32_t)addr - DISKMAP) / BLKSIZE;
+       int r;
 
        if (addr < (void*)DISKMAP || addr >= (void*)(DISKMAP + DISKSIZE))
                panic("flush_block of bad va %08x", addr);
 
        // LAB 5: Your code here.
-       panic("flush_block not implemented");
+
+       // Check if write needed
+       if (!va_is_mapped(addr) || !va_is_dirty(addr)) return;
+
+       // Block mapped and dirty
+       uint32_t secno = blockno * BLKSIZE / SECTSIZE;
+       addr = ROUNDDOWN(addr, PGSIZE);
+       ide_write(secno, addr, 1);
+
+       // This is copied from bg_pgfault. Why this clears the dirty bit?
+       // The originally mapped page is removed if any, and the new page
+       // is mapped, so here the content of the page is not changed at
+       // all, but the PTE in the page table is changed.
+       // upvt[PGNUM(addr)] gives the PTE of addr. Logical anding with
+       // PTE_SYSCALL discards any other flags except PTE_SYSCALL, so the
+       // dirty bit is cleared.
+       if ((r = sys_page_map(0, addr, 0, addr, uvpt[PGNUM(addr)] & PTE_SYSCALL)) < 0)
+               panic("in bc_pgfault, sys_page_map: %e", r);
 }
```

Note: this code passes the test but has bug. Refer to exercise 7.



## The Block Bitmap

> **Exercise 3.** Use `free_block` as a model to implement `alloc_block` in `fs/fs.c`, which should find a free disk block in the bitmap, mark it used, and return the number of that block. When you allocate a block, you should immediately flush the changed bitmap block to disk with `flush_block`, to help file system consistency.

Note that 1 means free, 0 means allocated. Read comment on `bitmap[blockno/32] & (1 << (blockno%32))`.

```diff
$ git diff
diff --git a/fs/fs.c b/fs/fs.c
index 45ecaf8..7efb211 100644
--- a/fs/fs.c
+++ b/fs/fs.c
@@ -31,6 +31,20 @@ block_is_free(uint32_t blockno)
 {
        if (super == 0 || blockno >= super->s_nblocks)
                return 0;
+
+       // bitmap is declared as: uint32_t *bitmap. Each bit
+       // represents one block (not sector).
+       // 1. Byte position
+       // blockno / 32 = blockno >> 5.
+       // bitmap[blockno/32] = *(bitmap + 4 * (blockno/32))
+       //                    = *(uint32_t *)((uint)bitmap + blockno/8).
+       // blockno/8 is the byte index of the bit in bitmap.
+       // If blockno = 9, then (bitmap + 1) is the address of the byte in
+       // bitmap that contains the bit for block 9.
+       // 2. Bit position within byte
+       // If blockno = 33, 1 << (blockno % 32) = 00000000 00000000 00000000 00000010.
+       //
+       // So bitmap[blockno/32] & (1 << (blockno % 32)) is 0 if free, 1 if allocated.
        if (bitmap[blockno / 32] & (1 << (blockno % 32)))
                return 1;
        return 0;
@@ -62,7 +76,21 @@ alloc_block(void)
        // super->s_nblocks blocks in the disk altogether.
 
        // LAB 5: Your code here.
-       panic("alloc_block not implemented");
+       if (super == NULL) panic("Cannot find super block");
+
+       for (int i = 0; i < super->s_nblocks; i++) {
+               // Note that 1 means free, while 0 means allocated.
+               uint8_t bit = bitmap[i/32] & (1 << (i%32));
+
+               if (bit) {
+                       // free block found, mark it as allocated by changing bit to 0
+                       bitmap[i/32] &= ~(1<<(i%32));
+                       void *addr = &bitmap[i/32];
+                       flush_block(addr);
+                       return i;
+               }
+       }
+
        return -E_NO_DISK;
 }
```



## File Operations

> **Exercise 4.** Implement `file_block_walk` and `file_get_block`. `file_block_walk` maps from a block offset within a file to the pointer for that block in the `struct File` or the indirect block, very much like what `pgdir_walk` did for page tables. `file_get_block` goes one step further and maps to the actual disk block, allocating a new one if necessary.

```c
static int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc)
{
	// LAB 5: Your code here.

	// Similar to xv6-public/fs.c:bmap
	int allocated_blockno;
	int res;

	cprintf("filebno: %u\n");
	if (filebno < NDIRECT) {
		if (ppdiskbno)
			*ppdiskbno = &(f->f_direct[filebno]);
		return 0;
	}
	filebno -= NDIRECT;

	if (filebno < NINDIRECT) {
		if (f->f_indirect == 0) {
			if (!alloc)
				return -E_NOT_FOUND;

			if ((res = allocated_blockno = alloc_block()) < 0)
				return res;

			void *addr = diskaddr(allocated_blockno);
			sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W);
			f->f_indirect = allocated_blockno;
			memset(addr, 0, BLKSIZE);

			flush_block(f);
			flush_block(addr);
		}

		if (ppdiskbno) {
			uint32_t *idblock = diskaddr(f->f_indirect);
			*ppdiskbno = &idblock[filebno];
			return 0;
		}
	}

	return -E_INVAL;
}

int
file_get_block(struct File *f, uint32_t filebno, char **blk)
{
	// LAB 5: Your code here.
	uint32_t *pdiskbno;
	int res;

    if ((res = file_block_walk(f, filebno, &pdiskbno, 1)) < 0)
		return res;

	if (pdiskbno && *pdiskbno == 0) {
		int allocated_blockno;

		if ((res = allocated_blockno = alloc_block()) < 0)
			return res;
		sys_page_alloc(0, diskaddr(allocated_blockno), PTE_P|PTE_U|PTE_W);
		*pdiskbno = allocated_blockno;
	}

	*blk = diskaddr(*pdiskbno);
	return 0;
}
```



## The file system interface

We have built the functionality within the file system environment and we now make it accessible to other environments. We expose access via *remote procedure call* (RPC) abstraction, built upon JOS's IPC mechanism. 

```
			Regular env           FS env
       +---------------+   +---------------+
       |      read     |   |   file_read   |
       |   (lib/fd.c)  |   |   (fs/fs.c)   |
  .....|.......|.......|...|.......^.......|...............
       |       v       |   |       |       | RPC mechanism
       |  devfile_read |   |  serve_read   |
       |  (lib/file.c) |   |  (fs/serv.c)  |
       |       |       |   |       ^       |
       |       v       |   |       |       |
       |     fsipc     |   |     serve     |
       |  (lib/file.c) |   |  (fs/serv.c)  |
       |       |       |   |       ^       |
       |       v       |   |       |       |
       |   ipc_send    |   |   ipc_recv    |
       |       |       |   |       ^       |
       +-------|-------+   +-------|-------+
               |                   |
               +-------------------+
```

Everything below the dotted line is the mechanics of getting a read request from a regular environment to the file system environment. `read` calls `devfile_read` using the `dev_read` field of `struct Dev *dev`. `devfile_read` and the other `devfile_*` in `lib/flie.c` implement the client side of the FS operations and work in similar way, bundling up arguments in a request structure, calling `fsipc` to send the IPC request, and unpacking and returnning the result. The `fsipc` handles the details of IPC. The file system server code is in `fs/serv.c`. It loops in the `serve` function, receiving a request over IPC, dispatching the request to the corresponding handler, and sending the resutl back via IPC. 

**Request**: JOS's IPC lets an environment send a single 32-bit number and optionally share a page. To send a request from the client to the server, we use the 32-bit number for the request type and store arguments to the request in a `union Fsipc` on the page shared. On the client side, we always share the page at `fsipcbuf`, on the server side, we map the incoming request page at `fsreq`, 0x0FFFF000. 

**Reply**: The server sends the response back via IPC. The 32-bit number is used for the return value. For most RPCs, this's all they return. `FSREQ_READ` and `FSREQ_STAT` also return data, using the page that the client sent its request. The page is shared between the client and the server, so the server just writes to the page. 

> **Exercise 5.** Implement `serve_read` in `fs/serv.c`.
>
> `serve_read`'s heavy lifting will be done by the already-implemented `file_read` in `fs/fs.c` (which, in turn, is just a bunch of calls to `file_get_block`). `serve_read` just has to provide the RPC interface for file reading. Look at the comments and code in `serve_set_size` to get a general idea of how the server functions should be structured.

Follow the structure of `serve_set_size`. Use the `fd_offset` field in `struct Fd`.

```c
int
serve_read(envid_t envid, union Fsipc *ipc)
{
	struct Fsreq_read *req = &ipc->read;
	struct Fsret_read *ret = &ipc->readRet;

	if (debug)
		cprintf("serve_read %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	// Lab 5: Your code here:
	struct OpenFile *o;
	int r;

	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;

	return file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset);
}
```

> **Exercise 6.** Implement `serve_write` in `fs/serv.c` and `devfile_write` in `lib/file.c`.

```c
static ssize_t
devfile_write(struct Fd *fd, const void *buf, size_t n)
{
	int r;

	fsipcbuf.write.req_fileid = fd->fd_file.id;
	fsipcbuf.write.req_n = n;
	memcpy(fsipcbuf.write.req_buf, buf, n);
	if ((r = fsipc(FSREQ_WRITE, NULL)) < 0)
		return r;
	assert(r <= n);
	return r;
}

int
serve_write(envid_t envid, struct Fsreq_write *req)
{
	if (debug)
		cprintf("serve_write %08x %08x %08x\n", envid, req->req_fileid, req->req_n);

	struct OpenFile *o;
	int r;

	if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
		return r;
	
	r = file_write(o->o_file, req->req_buf, req->req_n, o->o_fd->fd_offset);
	if (r > 0)
		o->o_fd->fd_offset += r;
	return r;
}
```

When doing this exercise, I found that test on "big file" cannot pass. The reason is that I forgot to increment `fd_offset` in `serve_read` previously, though it passed the test. I guess the reason is that there's no repeative read on the same file until the test on "big file".

```diff
$ git diff
diff --git a/fs/serv.c b/fs/serv.c
index e00cc86..5233067 100644
--- a/fs/serv.c
+++ b/fs/serv.c
@@ -220,7 +220,11 @@ serve_read(envid_t envid, union Fsipc *ipc)
        if ((r = openfile_lookup(envid, req->req_fileid, &o)) < 0)
                return r;
 
-       return file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset);
+       r = file_read(o->o_file, ret->ret_buf, req->req_n, o->o_fd->fd_offset);
+       if (r > 0)
+               o->o_fd->fd_offset += r;
+       return r;
 }
```



## Spawning Processes

In `lib/spawn.c`, `spawn` creates a new environement, loads a program image into it, and then starts the child environment running the program. The parent process runs independently of the child. The `spawn` effectively acts like a `fork` followed by an immediate `exec` in the child process. 

We implemented `spawn` , instead of Unix-style `exec`, as `spawn` is easier to implement from user space, without special help from the kernel. 

> **Exercise 7.** `spawn` relies on the new syscall `sys_env_set_trapframe` to initialize the state of the newly created environment. Implement `sys_env_set_trapframe` in `kern/syscall.c` (don't forget to dispatch the new system call in `syscall()`).

```c
static int
sys_env_set_trapframe(envid_t envid, struct Trapframe *tf)
{
	// LAB 5: Your code here.
	struct Env *e;
	pte_t *pte;
	int res;

	if ((res = envid2env(envid, &e, 1)) < 0) return res;
	pte = pgdir_walk(curenv->env_pgdir, (void *)tf, 0);
	if (!pte || !(*pte & PTE_P) || !(*pte & PTE_U)) panic("Invalid tf");

	e->env_tf = *tf;
	e->env_tf.tf_cs |= 0x3;
	e->env_tf.tf_eflags |= FL_IF;
	e->env_tf.tf_eflags &= ~FL_IOPL_3;
	return 0;
}
```

Note the way to set IOPL to 0.

After implementing this, I keeps getting this panic:

```
[00001001] user panic in <unknown> at user/spawnhello.c:9: spawn(hello) failed: file or block not found
```

After long time of debugging, I found the reason: a subtle bug in exercise 2. For `bc_pgfault` and `flush_block` in `bc.c`, the argument to `ide_read` and `ide_write` should be `BLKSECTS` (number of sectors per block), instead of `1`. The file system operates at **block** granularity! 

```diff
$ git diff
diff --git a/fs/bc.c b/fs/bc.c
index 6184553..ee15d2c 100644
--- a/fs/bc.c
+++ b/fs/bc.c
@@ -48,11 +48,10 @@ bc_pgfault(struct UTrapframe *utf)
        uint32_t secno = blockno * BLKSIZE / SECTSIZE;
        addr = ROUNDDOWN(addr, PGSIZE);
        sys_page_alloc(0, addr, PTE_P|PTE_U|PTE_W);
-       ide_read(secno, addr, 1);
+       ide_read(secno, addr, BLKSECTS);
 
        // Clear the dirty bit for the disk block page since we just read the
        // block from disk
@@ -90,7 +89,7 @@ flush_block(void *addr)
        // Block mapped and dirty
        uint32_t secno = blockno * BLKSIZE / SECTSIZE;
        addr = ROUNDDOWN(addr, PGSIZE);
-       ide_write(secno, addr, 1);
+       ide_write(secno, addr, BLKSECTS);
```



### Sharing library state across fork and spawn

The UNIX file descriptors are a general notion that also encompasses pipes, console I/O, etc. In JOS, each of these device types has a corresponding `struct Dev`, with pointers to the functions that implement read/write/etc for that device type. `lib/fd.c` implements the general UNIX-like file descriptor interface on top of this.

`lib/fd.c` maintains the *file descriptor table* region in each application environment's address space, starting at `FDTABLE`. This area reserves a page for each file descriptors (at most `MAXFD`) the application can open at once. A particular file descriptor table page is mapped if and only if the corresponding file descriptor is in use. 

We'd like file descriptor state to be shared across `fork` and `spawn`, but file descriptor table is in user-space memory: `FDTABLE` = 0xD0000000, while `KERNBASE` = 0xF0000000. Currently, `fork` marks the memory copy-on-wirte, so the state would be duplicated rather than shared; `spawn` (read the code and comment for `spawn`) creates a new address space so the spawned environment starts with no open file descriptors. 

We'll change `fork` to know that certain memory regions are used by "library OS" and should be shared. We set a new bit, `PTE_SHARE`, to indicate this information. If set, the PTE should be copied directly from parent to child in both `fork` and `spawn`. We want to *share* updates to the page. 

> **Exercise 8.** Change `duppage` in `lib/fork.c` to follow the new convention. If the page table entry has the `PTE_SHARE` bit set, just copy the mapping directly. (You should use `PTE_SYSCALL`, not `0xfff`, to mask out the relevant bits from the page table entry. `0xfff` picks up the accessed and dirty bits as well.)
>
> Likewise, implement `copy_shared_pages` in `lib/spawn.c`. It should loop through all page table entries in the current process (just like `fork` did), copying any page mappings that have the `PTE_SHARE` bit set into the child process.

```diff
diff --git a/lib/fork.c b/lib/fork.c
index e5365df..06541be 100644
--- a/lib/fork.c
+++ b/lib/fork.c
@@ -86,7 +86,10 @@ duppage(envid_t envid, unsigned pn)
 
-       if ((pte & PTE_W) || (pte & PTE_COW)) {
+       if (pte & PTE_SHARE) {
+               if ((r = sys_page_map(0, va, envid, va, PTE_P|PTE_U|PTE_W|PTE_SHARE)) < 0)
+                       panic("duppage: %e\n", r);
+       } else if ((pte & PTE_W) || (pte & PTE_COW)) {
                if ((r = sys_page_map(0, va, envid, va, PTE_U|PTE_COW|PTE_P)) < 0)
                        panic("duppage: cannot map COW into new env. %e\n", r);
 
diff --git a/lib/spawn.c b/lib/spawn.c
index 9d0eb07..7a13141 100644
--- a/lib/spawn.c
+++ b/lib/spawn.c
@@ -302,6 +302,21 @@ static int
 copy_shared_pages(envid_t child)
 {
        // LAB 5: Your code here.
+       extern volatile pte_t uvpt[];     // VA of "virtual page table"
+       extern volatile pde_t uvpd[];     // VA of current page directory
+
+       for (uint32_t va = 0; va < UXSTACKTOP - PGSIZE ; va += PGSIZE) {
+               pde_t pde = uvpd[PDX(va)];
+               pte_t pte;
+               if (pde & PTE_P) {
+                       pte = uvpt[PGNUM(va)];
+                       if ((pte & PTE_P) && (pte & PTE_SHARE)) {
+                               sys_page_map(0, (void *)va, child, (void *)va, 
+											 PTE_P|PTE_U|PTE_W|PTE_SHARE);
+                       }
+               }
+       }
+       return 0;
+
        return 0;
 }
```

Two points worth of noticing: 

1. Read `testpteshare.c` and you see that `PTE_SHARE` can be set together with `PTE_W`. So in `duppage`, the check for `PTE_SHARE` should be **before** check for `PTE_W|PTE_COW`. 
2. In `copy_shared_pages`, PTE should be accessed only if PDE exists.



## The keyboard interface

> **Exercise 9.** In your `kern/trap.c`, call `kbd_intr` to handle trap `IRQ_OFFSET+IRQ_KBD` and `serial_intr` to handle trap `IRQ_OFFSET+IRQ_SERIAL`.

```c
if (tf->tf_trapno == IRQ_OFFSET + IRQ_KBD) {
    kbd_intr();
    return;
}

if (tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL) {
    serial_intr();
    return;
}
```



## The shell

> **Exercise 10.**
>
> The shell doesn't support I/O redirection. It would be nice to run sh <script instead of having to type in all the commands in the script by hand, as you did above. Add I/O redirection for < to `user/sh.c`.
>
> Test your implementation by typing sh <script into your shell
>
> Run make run-testshell to test your shell. `testshell` simply feeds the above commands (also found in `fs/testshell.sh`) into the shell and then checks that the output matches `fs/testshell.key`.

Follow the branch on `>`.

```c
case '<':	// Input redirection
    // Grab the filename from the argument list
    if (gettoken(0, &t) != 'w') {
        cprintf("syntax error: < not followed by word\n");
        exit();
    }

    if ((fd = open(t, O_RDONLY)) < 0) {
        cprintf("Open %s for read: %e", t, fd);
        exit();
    }

    if (fd != 0) {
        dup(fd, 0);
        close(fd);
    }
    break;
```

Note that the `icode` test could take long to finish.