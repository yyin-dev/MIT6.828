# Chapter 0 Operating system interfaces

## Processes and memory

- An xv6 process consists of user-space memory and per-process state private to the kernel.
- All xv6 processes run as root.

## I/O and File descriptors

- Internally, the xv6 kernel uses the file descriptor as an index into a per-process table, so that each process has a private space of file descriptors starting at zero. 

- Each file descriptor referring to a file has an associated offset. 

- A simple implementation of `cat`:

  ```c
  char buf[512];
  int n;
  for(;;){
      n = read(0, buf, sizeof buf);
      if(n == 0) break;
      if(n < 0){
      	fprintf(2, "read error\n");
      	exit();
      }
      if(write(1, buf, n) != n){
          fprintf(2, "write error\n");
          exit();
      }
  }
  ```

  Note that `cat` doesn't know whether it's reading from a file, console, or a pipe. It doesn't know whether it's writing to a file, console, or a pipe. The convention of file descriptors 0 and 1 allows a simple implementation. 

- `fork` copies the parent's file descriptor table and its memory, so the child starts with exactly the same open files as the parent. The system call `exec` replaces the calling process's memory but preserves its file table. A newly allocated file descriptor is always the lowest-numbered unused descriptor of the current process. These properties make I/O redirection easy to implement. 

  Note that although `fork` copies the file descriptor table, each underlying file offset is shared between parent and child. The `dup` system call also maintains the offset.

  ```c
  // helloWorld1.c
  
  if(fork() == 0) {
      write(1, "hello ", 6);
      exit();
  } else {
      wait();
      write(1, "world\n", 6);
  }
  ```

  ```c
  // helloWorld2.c
  
  fd = dup(1);
  write(1, "hello ", 6);
  write(fd, "world\n", 6);
  ```

  Two file descriptors share an offset if they were derived from the same original file descriptor by a sequence of `fork` and `dup` calls. Otherwise, file descriptors don't share offsets, even if they resulted from `open` calls to the same file. 

## Pipe

A `pipe` is a small kernel buffer exposed to processes as a pair of file descriptors. Pipes provide a way for processes to communicate.

```c
int pipe(int pipefd[2]); // pipefd[0] is read end, pipefd[1] is write end
```

A `read` on a pipe blocks until data to be written to the write end, or all file descriptors referring to the write end to be closed. Thus, it's important for the reader process to close the write end.

Pipe's advantage over using temporary files:

1. Pipes automatically clean themselves up, but temporary files must be manually removed.
2. Pipes can pass arbitrarily long streams of data.
3. Pipes allow parallel execution of pipeline stages.

## File system

A file's name is distinct from the file itself. The same underlying file, called an `inode`, can have multiple names, called `links`. The `link` system call creates another name referring to the same inode. 

```c
open("a.txt", O_CREATE|OWRONLY);
link("a.txt", "b.txt");
```

The `unlink` system call removes a name from the file system. The file's inode and the disk space holding the content are only freed when the file's link count is zero and no file descriptors refer to it.

`mkdir`, `ls`, `rm`, etc are implemented as user-level programs. Only `cd` is built into the shell. 