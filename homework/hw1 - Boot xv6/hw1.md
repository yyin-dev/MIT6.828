# HW1

- In `bootasm.S`, `movl $start, %esp` initializes the stack. In `bootblock.asm`, it's:

  ```assembly
  movl    $start, %esp
    7c43:	bc 00 7c 00 00       	mov    $0x7c00,%esp
  ```

  So as indicated in the xv6 book, the stack grows from `0x7c00`.

  Before execute `call bootmain`:

  ```
  (gdb) info reg
  eax            0x0      0
  ecx            0x0      0
  edx            0x80     128
  ebx            0x0      0
  esp            0x7c00   0x7c00
  ebp            0x0      0x0
  esi            0x0      0
  edi            0x0      0
  eip            0x7c48   0x7c48
  eflags         0x6      [ PF ]
  cs             0x8      8
  ss             0x10     16
  ds             0x10     16
  es             0x10     16
  fs             0x0      0
  gs             0x0      0
  ```

  At this stage, the stack is still empty. Nothing has been pushed onto it.

- `bootmain` function

  ```assembly
  00007d3b <bootmain>:
  {
      7d3b:	55                   	push   %ebp
      7d3c:	89 e5                	mov    %esp,%ebp
      7d3e:	57                   	push   %edi
      7d3f:	56                   	push   %esi
      7d40:	53                   	push   %ebx
      7d41:	83 ec 0c             	sub    $0xc,%esp
    readseg((uchar*)elf, 4096, 0);
      7d44:	6a 00                	push   $0x0
      7d46:	68 00 10 00 00       	push   $0x1000
      7d4b:	68 00 00 01 00       	push   $0x10000
      7d50:	e8 a3 ff ff ff       	call   7cf8 <readseg>
    if(elf->magic != ELF_MAGIC)
      7d55:	83 c4 0c             	add    $0xc,%esp
      7d58:	81 3d 00 00 01 00 7f 	cmpl   $0x464c457f,0x10000
      7d5f:	45 4c 46 
      7d62:	74 08                	je     7d6c <bootmain+0x31>
  }
      7d64:	8d 65 f4             	lea    -0xc(%ebp),%esp
      7d67:	5b                   	pop    %ebx
      7d68:	5e                   	pop    %esi
      7d69:	5f                   	pop    %edi
      7d6a:	5d                   	pop    %ebp
      7d6b:	c3                   	ret    
    ph = (struct proghdr*)((uchar*)elf + elf->phoff);
      7d6c:	a1 1c 00 01 00       	mov    0x1001c,%eax
      7d71:	8d 98 00 00 01 00    	lea    0x10000(%eax),%ebx
    eph = ph + elf->phnum;
      7d77:	0f b7 35 2c 00 01 00 	movzwl 0x1002c,%esi
      7d7e:	c1 e6 05             	shl    $0x5,%esi
      7d81:	01 de                	add    %ebx,%esi
    for(; ph < eph; ph++){
      7d83:	39 f3                	cmp    %esi,%ebx
      7d85:	72 0f                	jb     7d96 <bootmain+0x5b>
    entry();
      7d87:	ff 15 18 00 01 00    	call   *0x10018
      7d8d:	eb d5                	jmp    7d64 <bootmain+0x29>
    for(; ph < eph; ph++){
      7d8f:	83 c3 20             	add    $0x20,%ebx
      7d92:	39 de                	cmp    %ebx,%esi
      7d94:	76 f1                	jbe    7d87 <bootmain+0x4c>
      pa = (uchar*)ph->paddr;
      7d96:	8b 7b 0c             	mov    0xc(%ebx),%edi
      readseg(pa, ph->filesz, ph->off);
      7d99:	ff 73 04             	pushl  0x4(%ebx)
      7d9c:	ff 73 10             	pushl  0x10(%ebx)
      7d9f:	57                   	push   %edi
      7da0:	e8 53 ff ff ff       	call   7cf8 <readseg>
      if(ph->memsz > ph->filesz)
      7da5:	8b 4b 14             	mov    0x14(%ebx),%ecx
      7da8:	8b 43 10             	mov    0x10(%ebx),%eax
      7dab:	83 c4 0c             	add    $0xc,%esp
      7dae:	39 c1                	cmp    %eax,%ecx
      7db0:	76 dd                	jbe    7d8f <bootmain+0x54>
        stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
      7db2:	01 c7                	add    %eax,%edi
      7db4:	29 c1                	sub    %eax,%ecx
  }
  ```

  Before executing `call   7cf8 <readseg>`, the stack looks like: 

  ```
  (gdb) x/12x $esp
  0x7bd4: 0x00010000      0x00001000      0x00000000      0x00000000
  0x7be4: 0x00000000      0x00000000      0x00000000      0x00000000
  0x7bf4: 0x00000000      0x00000000      0x00007c4d      0x8ec031fa
  ```

  Notice that `0x00007c4d` is pushed onto stack by `call`. 

  After `0x7d55  add    $0xc,%esp`, the stack looks like:

  ```
  (gdb) x/12x $esp
  0x7be0: 0x00000000      0x00000000      0x00000000      0x00000000
  0x7bf0: 0x00000000      0x00000000      0x00000000      0x00007c4d
  0x7c00: 0x8ec031fa      0x8ec08ed8      0xa864e4d0      0xb0fa7502
  ```

  This matches the assembly. There're in total 7 `0x00000000` on the stack. Four for saved registers and three for `7d41	sub    $0xc,%esp`.

- `7d87    call   *0x10018` is code that calls `entry()`. 

  ```
  (gdb) break * 0x7d87
  Breakpoint 2 at 0x7d87
  (gdb) c
  Continuing.
  => 0x7d87:      call   *0x10018
  
  Thread 1 hit Breakpoint 2, 0x00007d87 in ?? ()
  (gdb) x/12x $esp
  0x7be0: 0x00000000      0x00000000      0x00000000      0x00000000
  0x7bf0: 0x00000000      0x00000000      0x00000000      0x00007c4d
  0x7c00: 0x8ec031fa      0x8ec08ed8      0xa864e4d0      0xb0fa7502
  (gdb) x/w 0x10018
  0x10018:        0x0010000c
  ```

  So we see that `0x10018` stores the memory address of the code of `entry`, `0x0010000c`. 

  ```
  (gdb) si
  => 0x10000c:    mov    %cr4,%eax
  0x0010000c in ?? ()
  (gdb) x/24x $esp
  0x7bdc: 0x00007d8d      0x00000000      0x00000000      0x00000000
  0x7bec: 0x00000000      0x00000000      0x00000000      0x00000000
  0x7bfc: 0x00007c4d      0x8ec031fa      0x8ec08ed8      0xa864e4d0
  0x7c0c: 0xb0fa7502      0xe464e6d1      0x7502a864      0xe6dfb0fa
  0x7c1c: 0x16010f60      0x200f7c78      0xc88366c0      0xc0220f01
  0x7c2c: 0x087c31ea      0x10b86600      0x8ed88e00      0x66d08ec0
  ```

  `0x00007d8d` appears on the stack as that's the return address pushed by `call`. 

  

## Final answer

```
(gdb) x/24x $esp
0x7bdc: 0x00007d8d      0x00000000      0x00000000      0x00000000
0x7bec: 0x00000000      0x00000000      0x00000000      0x00000000
0x7bfc: 0x00007c4d      0x8ec031fa      0x8ec08ed8      0xa864e4d0
0x7c0c: 0xb0fa7502      0xe464e6d1      0x7502a864      0xe6dfb0fa
0x7c1c: 0x16010f60      0x200f7c78      0xc88366c0      0xc0220f01
0x7c2c: 0x087c31ea      0x10b86600      0x8ed88e00      0x66d08ec0
```

Valid part:

```
0x7bdc: 0x00007d8d      0x00000000      0x00000000      0x00000000
0x7bec: 0x00000000      0x00000000      0x00000000      0x00000000
0x7bfc: 0x00007c4d 
```

`0x00007d8d` is return address after calling `entry()`. `0x00007c4d` is return address after calling `bootmain()`.