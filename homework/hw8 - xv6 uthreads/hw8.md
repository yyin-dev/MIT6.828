# HW8

Output on my machine:

```
$ uthread
my thread running
my thread 0x2DC8
my thread running
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
...
my thread 0x4DD0
my thread 0x2DC8
my thread 0x4DD0
my thread: exit
my thread: exit
thread_schedule: no runnable threads
$ 
```



> What address is `0xd8`, which sits on the top of the stack of `next_thread`?

On my machine, the output is

```
(gdb) x/9x next_thread->sp
0x4da8:	0x00000000	0x00000000	0x00000000	0x00000000
0x4db8:	0x00000000	0x00000000	0x00000000	0x00000000
0x4dc8:	0x00000190
```

and from `uthread.asm`, we know

```
00000190 <mythread>:
{
 190:	55                   	push   %ebp
 191:	89 e5                	mov    %esp,%ebp
 193:	53                   	push   %ebx
  printf(1, "my thread running\n");
 194:	bb 64 00 00 00       	mov    $0x64,%ebx
{
 199:	83 ec 0c             	sub    $0xc,%esp
  printf(1, "my thread running\n");
 19c:	68 20 0a 00 00       	push   $0xa20
 ...
```

So `0x190` is the first instruction of `next_thread` in `my_thread`. This makes sense since this would be the first time `next_thread` gets scheduled.