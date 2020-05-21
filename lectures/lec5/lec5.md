# Lec 5

- Hardware isolation mechanisms: 
  - user/kernel mode
  - address spaces
  - timeslicing
  - syscall interface.



## User/Kernel mode

- On x86, CPL, the bottom two bits of %cs, controls user/kernel mode. 

  CPL = 0, kernel mode. CPL = 3, user mode. CPL protects processor registers. 

- How to make a system call? Setting CPL and calling/returning from syscall must be a combined instruction to guard malicous users. The syscall vector. 

  For example. INT instruction sets CPL = 0 and jumps to an entry point; Returning from system call sets CPL = 3 before returning to user code. 



## Address space isolation

Xv6 uses x86 "paging hardware" in MMU, which translates virtual address to physical address. Note that MMU translates all memory references: user and kernel, instructions and data.



## Xv6 syscall implementation

- Two mechanisms:

  - switch between user/kernel code
  - switch between kernel threads

- Xv6 address space recap

  `[0x00000000, 0x80000000]` is for user instruction, data and stack. `[0x80000000, 0xFFFFFFFF]` is for kernel. MMU ensures that user code (CPL = 3) can only access the lower half of the address space of its own process, but the kernel mappings are the same for every process. 

- Consider how `sh.c` writes its "$ " prompt

  ```c
  // sh.c
  int getcmd(char *buf, int nbuf) {
    printf(2, "$ ");
    // ...
  }
  
  // printf.c
  void printf(int fd, const char *fmt, ...) {
    // ...
    putc(fd, c);
    // ...
  }
  
  // printf.c
  static void putc(int fd, char c) {
    write(fd, &c, 1);
  }
  
  // user.h
  int write(int, const void*, int);
  
  // syscall.h
  #define SYS_write  16
  
  // usys.S
  #define SYSCALL(name) \
    .globl name; \
    name: \
      movl $SYS_ ## name, %eax; \
      int $T_SYSCALL; \
      ret
  
  // sh.asm
  00000d32 <write>:
  SYSCALL(write)
       d32:	b8 10 00 00 00       	mov    $0x10,%eax
       d37:	cd 40                	int    $0x40
       d39:	c3                   	ret   
  ```

  ```
  (gdb) symbol-file _sh
  Load new symbol table from "_sh"? (y or n) y
  Reading symbols from _sh...done.
  (gdb) break write 
  Breakpoint 1 at 0xd32: file usys.S, line 16.
  (gdb) x/3x 0xd32
  0xd32 <write>:	0x00000000	0x00000000	0x00000000
  ```

  The above way of tracing doesn't work, seems `sh.c` hasn't been loaded into memory yet. Thus, we trace `printf` instead, which would ensure `write` would be loaded into memory.

  ```
  (gdb) symbol-file _sh
  Load new symbol table from "_sh"? (y or n) y
  Reading symbols from _sh...done.
  (gdb) break printf
  Breakpoint 1 at 0xe60: file printf.c, line 41.
  (gdb) c
  Continuing.
  Thread 2 hit Breakpoint 1, printf (fd=2, fmt=0x11b8 "$ ") at printf.c:41
  41	{
  (gdb) break write 
  Breakpoint 2 at 0xd32: file usys.S, line 16.
  (gdb) c
  Continuing.
  16	SYSCALL(write)
  (gdb) x/4i 0xd32
  => 0xd32 <write>:	mov    $0x10,%ax
     0xd35 <write+3>:	add    %al,(%bx,%si)
     0xd37 <write+5>:	int    $0x40
     0xd39 <write+7>:	ret 
  (gdb) info registers 
  ...
  esp            0x3f4c	0x3f4c
  ...
  eip            0xd32	0xd32 <write>
  ...
  cs             0x1b	27
  ...
  (gdb) ni
  [  1b: d37]    0xee7 <printf+135>:	add    %al,0x25f8(%bp,%di)
  0x00000d37	16	SYSCALL(write)
  (gdb) x/4i 0xd32
     0xd32 <write>:	mov    $0x10,%ax
     0xd35 <write+3>:	add    %al,(%bx,%si)
  => 0xd37 <write+5>:	int    $0x40
  ```

  And we reached syscall `write`. We see:

  - `0x10` in %eax is the syscall number for `write`

  - CPL = 3, %esp and %eip are at low addresses - still in user virtual address

  - When `int $0x40` is the next instruction, inspect the stack:

    ```
    (gdb) x/4x $esp
    0x3f4c:	0x00000ea5	0x00000002	0x00003f7a	0x00000001
    (gdb) x/c 0x00003f7a
    0x3f7a:	36 '$'
    ```

    `2` is the file descriptor, `0x3f5c` is the buffer on the stack, `1` is the count, i.e., `write(2, 0x3f5c, 1)`. `0x00000ea5` is the next instruction when return.

- Keep tracing into `INT` instruction

  ```
  (gdb) si
  The target architecture is assumed to be i386
  => 0x80105d39:	push   $0x40
  0x80105d39 in ?? ()
  (gdb) x/8i 0x80105d39
  => 0x80105d39:	push   $0x40
     0x80105d3b:	jmp    0x8010562a
     0x80105d40:	push   $0x0
     0x80105d42:	push   $0x41
     0x80105d44:	jmp    0x8010562a
     0x80105d49:	push   $0x0
     0x80105d4b:	push   $0x42
     0x80105d4d:	jmp    0x8010562a
  ```

  Clearly this's the syscall table. 

  ```
  (gdb) ni 2
  => 0x8010562a:	push   %ds
  0x8010562a in ?? ()
  (gdb) x/10i 0x8010562a
  => 0x8010562a:	push   %ds
     0x8010562b:	push   %es
     0x8010562c:	push   %fs
     0x8010562e:	push   %gs
     0x80105630:	pusha  
     0x80105631:	mov    $0x10,%ax
     0x80105635:	mov    %eax,%ds
     0x80105637:	mov    %eax,%es
     0x80105639:	push   %esp
     0x8010563a:	call   0x80105700
  (gdb) info registers 
  eax            0x10	16
  ecx            0x24	36
  edx            0x0	0
  ebx            0x24	36
  esp            0x8dffefe4	0x8dffefe4
  ebp            0x3f98	0x3f98
  esi            0x11b9	4537
  edi            0x0	0
  eip            0x8010562a	0x8010562a
  eflags         0x216	[ PF AF IF ]
  cs             0x8	8
  ss             0x10	16
  ds             0x23	35
  es             0x23	35
  fs             0x0	0
  gs             0x0	0
  (gdb) x/6wx $esp
  0x8dffefe4:	0x00000040	0x00000000	0x00000d39	0x0000001b
  0x8dffeff4:	0x00000216	0x00003f4c
  ```

  And we step into `trapasm.S`.

  - CPL = 0, kernel mode, %esp and %eip are at high address - kernel virtual address

  - INT would save user registers on the stack - err, eip, cs, eflags, esp, ss. INT only saves those since INT would overwrite them.
  - INT would:
    - Switch to current process's kernel stack
    - Save user register on the kernel stack
    - Set CPL = 0
    - Start executing at kernel supplied vector

`trapasm.S:alltraps` saves the rest of the user register on the kernel stack. Those would be restored when the syscall returns. `pushl %esp` creates the argument for `trap(struct trapframe *tf)`, then `trap` is called. 

Device interrupts and faults also enter `trap()`, so set `trapno = T_SYSCALL` to distinguish. `myproc()->tf = tf;` ensures `syscall()` can get call number and arguments. 

```c
// trap.c
void trap(struct trapframe *tf) {
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }
  ...
}

// syscall.c
void syscall(void) {
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n", curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}

static int (*syscalls[])(void) = {
...
[SYS_write]   sys_write,
...
};
```

When `syscall` returns, `trap` returns, `trapasm.S:trapret` is executed. At this stage, registers contain kernel values. `trapret` restores those values.

### System call summary

```
printf.c:putc -> usys.h -> trapasm.S -> trap.c:trap -> syscall.c:syscall -> sys_write -> trap.c:trap -> trapasm.S:trapret -> back to user space
```

`putc` calls `write`, which is defined in `usys.h`. `write` contains `INT` instruction, which jumps to `trapasm.S`, which in turn calls `trap`. `Trap` calls `syscall`, which calls the corresponding `sys_write`. After `sys_write` returns, `syscall` returns, `trap` returns, and `trapret` executes, and `iret` returns to the user space. 