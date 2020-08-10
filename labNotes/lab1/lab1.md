## Brennan's Guide to Inline Assembly [x]

- Operator size specification

  `b`: byte, 8 bits. `w`: word, 16 bits. `l`: longword, 32 bits.

- Referencing memory

  Format: `immedOffset(basePointer, indexPointer, indexScale)`

  Value: `immedOffset + basePointer + indexPointer  * indexScale`

  - Addressing a static (global) C variable from assembly

    `_foo`

    The underscore is required.

  - Addressing a value in an array of integers

    `_array(, %eax, 4)`

  - Addressing a char in an array of 8-byte records

    %eax holds the index of the record desired. %ebx holds the offset within the record.

    `_array(%ebx, %eax, 8)`



## Part1: PC Bootup

### The PC's Physical Address Space

```
8086 physical address space:

+------------------+  <- 0xFFFFFFFF (4GB)
|      32-bit      |
|  memory mapped   |
|     devices      |
/\/\/\/\/\/\/\/\/\/\

/\/\/\/\/\/\/\/\/\/\
|      Unused      |
+------------------+  <- depends on amount of RAM
|                  |
| Extended Memory  |
|                  |
+------------------+  <- 0x00100000 (1MB)
|     BIOS ROM     |
+------------------+  <- 0x000F0000 (960KB)
|  16-bit devices, |
|  expansion ROMs  |
+------------------+  <- 0x000C0000 (768KB)
|   VGA Display    |
+------------------+  <- 0x000A0000 (640KB)
|    Low Memory    |
+------------------+  <- 0x00000000
```

The first PC, 16-bit Intel 8088, can only address 1MB of physical memory, from 0x00000000 to 0x0000FFFF. In other words, the 640KB area marked as "Low Memory" is the only RAM that 8088 could use. The 384KB area from 0x000A0000 through 0x000FFFFF was reserved by the hardware. BIOS is stored in read-only memory (ROM). 

When Intel advanced from 8088 to 8086, the architecture is preserved for backward compatibility. So there's a "hole" in modern 8086 PC's physical memory, from 0x000A0000 to 0x00100000. Memory above 0x00100000 is called "extended memory". Also, some memory at the top of the 32-bit address space is also reserved by BIOS. If the processor can support more than 4GB of physical memory, this becomes the second "hole" in the physical memory address space. JOS only uses the first 256 MB of a PC's physical memory. 



### The ROM BIOS

Note that in x86 assembly AT&T syntax, register is prefixed with `%`. But in gdb, the prefix is `$`.

> **Exercise 2.** Use GDB's `si` (Step Instruction) command to trace into the ROM BIOS for a few more instructions, and try to guess what it might be doing. You might want to look at Phil Storrs I/O Ports Description, as well as other materials on the 6.828 reference materials page. No need to figure out all the details - just the general idea of what the BIOS is doing first.

```assembly
0xffff0:	ljmp   $0xf000,$0xe05b	# jump to earlier location

0xfe05b:	cmpl   $0x0,%cs:0x6ac8
0xfe062:	jne    0xfd2e1
0xfe066:	xor    %dx,%dx
0xfe068:	mov    %dx,%ss
0xfe06a:	mov    $0x7000,%esp
0xfe070:	mov    $0xf34c2,%edx
0xfe076:	jmp    0xfd15c

0xfd15c:	mov    %eax,%ecx
0xfd15f:	cli    					# disable interrupt
0xfd160:	cld    
0xfd161:	mov    $0x8f,%eax
0xfd167:	out    %al,$0x70
0xfd169:	in     $0x71,%al
0xfd16b:	in     $0x92,%al
0xfd16d:	or     $0x2,%al
0xfd16f:	out    %al,$0x92
0xfd171:	lidtw  %cs:0x6ab8
0xfd177:	lgdtw  %cs:0x6a74		# load gdt
0xfd17d:	mov    %cr0,%eax
0xfd180:	or     $0x1,%eax
0xfd184:	mov    %eax,%cr0
0xfd187:	ljmpl  $0x8,$0xfd18f	# ljmpl to enter real mode
```

The PC starts executing at 0x000FFFF0, this is designed by Intel 8088. This's just 16 bytes before the end of BIOS. So the first thing BIOS does is to jump backwards to an earlier location of BIOS.



## Part2: The Boot Loader

Disks are divided into 512-byte regions called *sector*s. A sector is the minimum transfer unit of disk. If the disk is bootable, the first sector is called the *boot sector*. BIOS loads the boot sector into memory at physical address 0x7c00 through 0x7dff, and uses a `jmp` instruction to set the `CS:IP` to `0000:7c00`, passing control to the boot loader. Theses addresses are designed arbitrarily by Intel.

For 6.828, the boot loader consists of `boot/boot.S` and `boot/main.c`. The boot loader performs two functions: (1) Switch from real mode to 32-bit protected mode. Only in protected mode can the processor access memory above 1MB; (2) Read the kernel from the disk into memory. `obj/boot/boot.asm` is the disassembly of the boot loader.  

> **Exercise 3.** 
>
> - At what point does the processor start executing 32-bit code? What exactly causes the switch from 16- to 32-bit mode?
> - What is the *last* instruction of the boot loader executed, and what is the *first* instruction of the kernel it just loaded?
> - *Where* is the first instruction of the kernel?
> - How does the boot loader decide how many sectors it must read in order to fetch the entire kernel from disk? Where does it find this information?

- The processor starts executing 32-bit code from `0x7c32:	mov    $0xd88e0010,%eax`. This is triggered by `0x7c2d:	ljmp   $0x8,$0x7c32`.

  ```assembly
  0x7c00:	cli    					# first boot loader instruction
  0x7c01:	cld    
  0x7c02:	xor    %ax,%ax
  0x7c04:	mov    %ax,%ds
  0x7c06:	mov    %ax,%es
  0x7c08:	mov    %ax,%ss
  0x7c0a:	in     $0x64,%al
  0x7c0c:	test   $0x2,%al
  0x7c0e:	jne    0x7c0a
  0x7c10:	mov    $0xd1,%al
  0x7c12:	out    %al,$0x64
  0x7c14:	in     $0x64,%al
  0x7c16:	test   $0x2,%al
  0x7c18:	jne    0x7c14
  0x7c1a:	mov    $0xdf,%al
  0x7c1c:	out    %al,$0x60
  0x7c1e:	lgdtw  0x7c64
  0x7c23:	mov    %cr0,%eax
  0x7c26:	or     $0x1,%eax
  0x7c2a:	mov    %eax,%cr0
  0x7c2d:	ljmp   $0x8,$0x7c32		# switch to 32-bit protected mode
  ```

- From `boot.asm`, we see that `7d6b: ff 15 18 00 01 00     call   *0x10018` is the last instruction of `bootmain()`. 

  ```
  (gdb) b * 0x7d6b
  Breakpoint 2 at 0x7d6b
  (gdb) c
  Continuing.
  The target architecture is assumed to be i386
  => 0x7d6b:	call   *0x10018
  (gdb) x/x (0x10018)
  0x10018:	0x0010000c
  (gdb) x/i 0x10000c
     0x10000c:	movw   $0x1234,0x472
  ```

  So we know that the last instruction of boot loader executed is `call 	*0x10018`, and the address stored at `0x10018` is `0x10000c`. So we know the first instruction of kernel code is `movw   $0x1234,0x472`. This matches with `obj/kernel/kernel.asm`.

- The instruction is at `0x10000c`.

- It finds the information from the ELF header. The logic is in `boot/main.c`.

### Loading the Kernel

> **Exercise 4.** Easy C language exericse.

When compiling and linking a C program, the compiler transforms source file `.c` into *object file* `.o`. The object file contains assembly instructions encoded in binary format (0's and 1's). The linker combines all object files into a single *binary image* like `obj/kern/kernel` in the ELF format. ELF: Executable and Linkable Format.

For 6.828, consider an ELF executable as a header with loading information, followed by several *program sections*, each being a contiguous chunk of code or data to be loaded at a specified memory address. The boot loader loads the ELF executable into memory and starts executing.  

An ELF binary starts with a fixed-length *ELF header*, followed by a variable-length *program header*, listing program sections to be loaded. C definitions for headers are in `inc/elf.h`. We are interested in the following program sections:

- `.text`: the program's executable instructions.
- .`rodata`: read-only data, like ASCII string constants produced by the C compiler.
- `.data`: the program's initialized data, like *initialized* global variables.  
- `.bss`: space for *uninitialized* global variables, the space is zeroed.

When linker computes the memory layout of the program, it reserves space for *unitialized* global variables in the `.bss` section, immediately after `.data`. C requires that unitialized global variables start with value of zero. There's no need to store contents for `.bss` in the ELF binary. Instead, the linker records the address and size of the `.bss` section. The loader arranges to zero the `.bss` section when loading the executable into the memory.

```
$ objdump -h obj/kern/kernel
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         000019e9  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .rodata       000006c0  f0101a00  00101a00  00002a00  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  ...
  4 .data         00009300  f0108000  00108000  00009000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  ...
  9 .bss          00000648  f0113060  00113060  00014060  2**5
                  CONTENTS, ALLOC, LOAD, DATA
 10 .comment      00000029  00000000  00000000  000146a8  2**0
                  CONTENTS, READONLY
```

Note that VMA (link address) and LMA (load address) of the `.text` section. The load address is the address at which the section should be loaded into memory. The link address is the address from which the section expects to execute. Typically the link and load address are the same, like `obj/boot/boot.out`. The boot loader uses the ELF program header to decide how to load sections. 

In `boot/main.c`, `ph->p_pa` of each program header is the destination physical address. The BIOS loads the boot sector, `boot.S` and `main.C`, into memory at 0x7c00. So this's the boot sector's load address. This's also where the boot sector starts executing, so it's also the link address. We set the link address by passing `-Ttext 0x7c00` to the linker in `boot/Makefrag`. 

> **Exercise 5.** Trace through the first few instructions of the boot loader again and identify the first instruction that would "break" or otherwise do the wrong thing if you were to get the boot loader's link address wrong. Then change the link address in `boot/Makefrag` to something wrong, run make clean, recompile the lab with make, and trace into the boot loader again to see what happens.
>
> PS: This exercise is complicated, but might not be important.

Of course, changing the link address of the boot loader doesn't affect BIOS at all. So we only have to worry about execution of boot loader.

Solution: https://zhuanlan.zhihu.com/p/36926462, https://www.cnblogs.com/fatsheep9146/p/5220004.html

From CSAPP, we know that when compiling source file into `.o` object file, not all addresses can be determined. Thus, *symbol resolution* is performed at linking to calculate addresses. 

After changing `-Ttext 0x7C00` to `-Ttext 0x8C00`. The program header is indeed changed:

```
$ objdump -h obj/boot/boot.out 

obj/boot/boot.out:     file format elf32-i386

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00000186  00008c00  00008c00  00000074  2**2
                  CONTENTS, ALLOC, LOAD, CODE
  1 .eh_frame     000000a8  00008d88  00008d88  000001fc  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         0000087c  00000000  00000000  000002a4  2**2
                  CONTENTS, READONLY, DEBUGGING
  3 .stabstr      00000925  00000000  00000000  00000b20  2**0
                  CONTENTS, READONLY, DEBUGGING
  4 .comment      00000029  00000000  00000000  00001445  2**0
                  CONTENTS, READONLY
```

However, the boot loader code is still loaded at `0x7c00` (this can be easily verified using gdb). Why is this the case? VMA and LMA controls the linker, but boot loader is loaded into memory by BIOS, which always loads the boot loader at 0x7c00.

```
(gdb) x/30i 0x7c00
=> 0x7c00:	cli    
   0x7c01:	cld    
   0x7c02:	xor    %ax,%ax
   0x7c04:	mov    %ax,%ds
   0x7c06:	mov    %ax,%es
   0x7c08:	mov    %ax,%ss
   0x7c0a:	in     $0x64,%al
   0x7c0c:	test   $0x2,%al
   0x7c0e:	jne    0x7c0a
   0x7c10:	mov    $0xd1,%al
   0x7c12:	out    %al,$0x64
   0x7c14:	in     $0x64,%al
   0x7c16:	test   $0x2,%al
   0x7c18:	jne    0x7c14
   0x7c1a:	mov    $0xdf,%al
   0x7c1c:	out    %al,$0x60
   0x7c1e:	lgdtw  -0x739c
   0x7c23:	mov    %cr0,%eax
   0x7c26:	or     $0x1,%eax
   0x7c2a:	mov    %eax,%cr0
   0x7c2d:	ljmp   $0x8,$0x8c32
   0x7c32:	mov    $0xd88e0010,%eax
   0x7c38:	mov    %ax,%es
   0x7c3a:	mov    %ax,%fs
   0x7c3c:	mov    %ax,%gs
   0x7c3e:	mov    %ax,%ss
   0x7c40:	mov    $0x8c00,%sp
   0x7c43:	add    %al,(%bx,%si)
   0x7c45:	call   0x7d13
   0x7c48:	add    %al,(%bx,%si)
```

we get:

```
Program received signal SIGTRAP, Trace/breakpoint trap.
[   0:7c2d] => 0x7c2d:	ljmp   $0x8,$0x8c32
```



Though we said that link address and load address are usually the same, like for `boot.out`, this's not true for `kernel`. The kernel would be loaded at a low address `0x00100000`, but expects to execute from a high address `0xf0100000`:

```
$ objdump -h obj/kern/kernel

obj/kern/kernel:     file format elf32-i386

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         000019e9  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  ...
```

The `e_entry` field in the ELF header is the link address of the *entry point* of the program: the memory address in the `.text` section at which the program should begin executing. This's exactly what `main.c` is doing.

```
$ objdump -f obj/kern/kernel

obj/kern/kernel:     file format elf32-i386
architecture: i386, flags 0x00000112:
EXEC_P, HAS_SYMS, D_PAGED
start address 0x0010000c
```

> **Exercise 6.** Examine the 8 words of memory at 0x00100000 at the point the BIOS enters the boot loader, and then again at the point the boot loader enters the kernel. Why are they different? What is there at the second breakpoint?

When BIOS enters boot loader, the memory location contains all zeros:

```
(gdb) br * 0x7c00
Breakpoint 1 at 0x7c00
(gdb) c
Continuing.
[   0:7c00] => 0x7c00:	cli    
(gdb) x/8w 0x100000
0x100000:	0x00000000	0x00000000	0x00000000	0x00000000
0x100010:	0x00000000	0x00000000	0x00000000	0x00000000
```

When the boot loader enters the kernel, the memory location contains the kernel code, loaded by the boot loader:

```
(gdb) b * 0x7d6b
Breakpoint 2 at 0x7d6b
(gdb) c
Continuing.
=> 0x7d6b:	call   *0x10018
(gdb) ni
=> 0x10000c:	movw   $0x1234,0x472
(gdb) x/8w 0x100000
0x100000:	0x1badb002	0x00000000	0xe4524ffe	0x7205c766
0x100010:	0x34000004	0x2000b812	0x220f0011	0xc0200fd8
(gdb) x/8wi 0x100000
   0x100000:	add    0x1bad(%eax),%dh
   0x100006:	add    %al,(%eax)
   0x100008:	decb   0x52(%edi)
   0x10000b:	in     $0x66,%al
   0x10000d:	movl   $0xb81234,0x472
   0x100017:	and    %dl,(%ecx)
   0x100019:	add    %cl,(%edi)
   0x10001b:	and    %al,%bl
```

This matches with `kernel.asm`.

At this stage, we finished discussion on boot loader.



## Part3: The Kernel

Like the boot loader, the kernel begins with some assembly code to set things up for C code.

### Using virtual memory to work around position dependence

The link address of kernel is `0xf0100000`, but the load address is `0x00100000`. OS kernels are usually loaded at very high *virtual address*, to leave the lower part of the *virtual address space* for user programs. We don't have paging at this stage, so we use the processor's memory management **hardware** to map virtual address `0xf0100000` (the link address of kernel) to physical address `0x00100000` (the load address of kernel). 

In this lab, we'll map the first 4MB of physical memory, from physical address `0x00000000` through `0x00400000`, to virtual addresses `0xf0000000` through `0xf0400000`. This is done using hand-written, statically-initialized page directory and page table in `kern/entrypgdir.c`. You don't need to understand how paging works for this lab, just the effect of it. 

Let's read `kern/entry.S`. Up until the `CR0_PG` is set, memory references are treated as physical addresses. Once `CR0_PG` is set, memory references are virtual addresses translated by the virtual memory hardware to physical addresses. `entry_pgdir` translates virtual addresses in the range `0xf0000000` through `0xf0400000` to physical addresses `0x00000000` through `0x00400000`, as well as virtual addresses `0x00000000` through `0x00400000` to physical addresses `0x00000000` through `0x00400000`. That is, both virtual address `[0x00000000, 0x00400000]` and virtual address `[0xf0000000, 0xf0400000]` are mapped to physical address `[0x00000000, 0x00400000]`. Any virtual address that's not in one of these two ranges would cause hardware exception. 

> **Exercise 7.** Trace into the JOS kernel and stop at the `movl %eax, %cr0`. Examine memory at 0x00100000 and at 0xf0100000. Now, single step over that instruction using the `stepi` GDB command. Again, examine memory at 0x00100000 and at 0xf0100000. Make sure you understand what just happened.
>
> What is the first instruction *after* the new mapping is established that would fail to work properly if the mapping weren't in place? Comment out the `movl %eax, %cr0` in `kern/entry.S`, trace into it, and see if you were right.

From Exercise 6, we know that the kernel code starts at `0x10000c`. 

```
(gdb) x/10i 0x10000c
=> 0x10000c:	movw   $0x1234,0x472
   0x100015:	mov    $0x112000,%eax
   0x10001a:	mov    %eax,%cr3
   0x10001d:	mov    %cr0,%eax
   0x100020:	or     $0x80010001,%eax
   0x100025:	mov    %eax,%cr0
   0x100028:	mov    $0xf010002f,%eax
   0x10002d:	jmp    *%eax
   0x10002f:	mov    $0x0,%ebp
   0x100034:	mov    $0xf0110000,%esp
(gdb) b * 0x100025
Breakpoint 3 at 0x100025
(gdb) c
Continuing.
=> 0x100025:	mov    %eax,%cr0
(gdb) x/x 0x00100000
0x100000:	0x1badb002
(gdb) x/x 0xf0100000
0xf0100000 <_start+4026531828>:	0x00000000
(gdb) si
=> 0x100028:	mov    $0xf010002f,%eax
(gdb) x/x 0x00100000
0x100000:	0x1badb002
(gdb) x/x 0xf0100000
0xf0100000 <_start+4026531828>:	0x1badb002
```

Before setting `%cr0`, memory references `0x0010000` and `0xf0100000` are physical addresses and contain different content. After setting `%cr0`, hardware paging is enabled and virtual addresses `0x00100000` and `0xf0100000` map to the same physical address: `0x00100000`. 

If mapping weren't in place (`movl %eax, %cr0` is commented), next instruction `jmp    *%eax` would fail, since it tries to jump to physical address `0xf010002f`. 

### Formatted Printing to the Console [x]

We have to implement `printf` ourselves, in `kern/printf.c`, `lib/printfmt.c,` `kern/console.c`. 

> **Exercise 8.** We have omitted a small fragment of code - the code necessary to print octal numbers using patterns of the form "%o". Find and fill in this code fragment.
>
> 1. Explain the interface between `printf.c` and `console.c`. Specifically, what function does `console.c` export? How is this function used by `printf.c`?
>
> 2. Explain the following from `console.c`:
>
>    ```c
>    1  if (crt_pos >= CRT_SIZE) {
>    2      int i;
>    3      memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE-CRT_COLS)*sizeof(uint16_t));
>    4      for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
>    5              crt_buf[i] = 0x0700 | ' ';
>    6      crt_pos -= CRT_COLS;
>    7  }
>    ```
>
> 3. For the following questions you might wish to consult the notes for Lecture 2. These notes cover GCC's calling convention on the x86.
>
>    Trace the execution of the following code step-by-step:
>
>    ```c
>    int x = 1, y = 3, z = 4;
>    cprintf("x %d, y %x, z %d\n", x, y, z);
>    ```
>
>    - In the call to `cprintf()`, to what does `fmt` point? To what does `ap` point?
>    - List (in order of execution) each call to `cons_putc`, `va_arg`, and `vcprintf`. For `cons_putc`, list its argument as well. For `va_arg`, list what `ap` points to before and after the call. For `vcprintf` list the values of its two arguments.
>
> 4. Run the following code.
>
>    ```c
>        unsigned int i = 0x00646c72;
>        cprintf("H%x Wo%s", 57616, &i);
>    ```
>
>    What is the output? Explain how this output is arrived at in the step-by-step manner of the previous exercise.
>
>    The output depends on that fact that the x86 is little-endian. If the x86 were instead big-endian what would you set `i` to in order to yield the same output? Would you need to change `57616` to a different value?
>
> 5. In the following code, what is going to be printed after `'y='`? Why does this happen?
>
>    ```c
>    cprintf("x=%d y=%d", 3);
>    ```

```c
case 'o': // very similar to `case 'u'`
    num = getuint(&ap, lflag);
    base = 8;
    goto number;
```

1. `console.c` also exports `cputchar`, used by `putch` function in `printf.c`.

2. Read the comment:

   ```c
   if (crt_pos >= CRT_SIZE) {
       int i;
   
       // void *memmove(void *dest, const void *src, size_t n);
       // The memmove() function copies n bytes from src to dest.
       // When the console is full, jump to a new empty row.
       memmove(crt_buf, crt_buf+CRT_COLS, (CRT_SIZE-CRT_COLS)*sizeof(uint16_t));
       for (i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++) 
           crt_buf[i] = 0x0700 | ' ';
       crt_pos -= CRT_COLS;
   }
   ```

3. Add code to `i386_init` in `init.c` for tracing.

   Command for printing variables: https://stackoverflow.com/a/6261502/9057530.

   ```
   $ make gdb
   (gdb) b i386_init 
   Breakpoint 1 at 0xf01000a6: file kern/init.c, line 24.
   (gdb) c
   Continuing.
   (gdb) disas i386_init 
   Dump of assembler code for function i386_init:
      ...
      0xf01000e3 <+61>:	call   0xf0100a59 <cprintf> # 1st cprintf call
      0xf01000e8 <+66>:	push   $0x4
      0xf01000ea <+68>:	push   $0x3
      0xf01000ec <+70>:	push   $0x1
      0xf01000ee <+72>:	lea    -0xf8b6(%ebx),%eax
      0xf01000f4 <+78>:	push   %eax
      0xf01000f5 <+79>:	call   0xf0100a59 <cprintf> # 2nd cprintf call
      ...
   (gdb) b * 0xf01000f5
   Breakpoint 2 at 0xf01000f5: file kern/init.c, line 40.
   (gdb) continue
   Continuing.
   => 0xf01000f5 <i386_init+79>:	call   0xf0100a59 <cprintf>
   
   Breakpoint 2, 0xf01000f5 in i386_init () at kern/init.c:40
   40		cprintf("x %d, y %x, z %d\n", x, y, z);
   (gdb) si
   => 0xf0100a59 <cprintf>:	push   %ebp
   cprintf (fmt=0xf0101a52 "x %d, y %x, z %d\n") at kern/printf.c:27
   27	{
   (gdb) info args 
   fmt = 0xf0101a52 "x %d, y %x, z %d\n"
   (gdb) disas cprintf 
   Dump of assembler code for function cprintf:
   => 0xf0100a59 <+0>:		push   %ebp
      0xf0100a5a <+1>:		mov    %esp,%ebp
      0xf0100a5c <+3>:		sub    $0x10,%esp
      0xf0100a5f <+6>:		lea    0xc(%ebp),%eax
      0xf0100a62 <+9>:		push   %eax			// push ap
      0xf0100a63 <+10>:	pushl  0x8(%ebp)	// push fmt
      0xf0100a66 <+13>:	call   0xf0100a22 <vcprintf>
      0xf0100a6b <+18>:	leave  
      0xf0100a6c <+19>:	ret
   (gdb) break * 0xf0100a66
   Breakpoint 3 at 0xf0100a66: file kern/printf.c, line 32.
   (gdb) c
   Continuing.
   => 0xf0100a66 <cprintf+13>:	call   0xf0100a22 <vcprintf>
   
   Breakpoint 3, 0xf0100a66 in cprintf (fmt=0xf0101a52 "x %d, y %x, z %d\n")
       at kern/printf.c:32
   32		cnt = vcprintf(fmt, ap);
   ```

   None of `info locals`, `info variables`, `print cprintf::ap` prints out `ap` correctly.

   ```
   (gdb) x/2x $esp
   0xf010ffb0:	0xf0101a52	0xf010ffd4
   (gdb) x/s 0xf0101a52
   0xf0101a52:	"x %d, y %x, z %d\n"
   (gdb) x/3wd 0xf010ffd4
   0xf010ffd4:	1	3	4
   ```

   Printing `ap` requires knowledge about `va_list` struct. This's not important and it's enough to see the idea. 

4. Add code to `i386_init` in `init.c` for tracing.

   ```
   (gdb) br * 0xf0100102
   Breakpoint 2 at 0xf0100102: file kern/init.c, line 44.
   (gdb) c
   Continuing.
   => 0xf0100102 <i386_init+92>:	call   0xf0100a68 <cprintf>
   
   Breakpoint 2, 0xf0100102 in i386_init () at kern/init.c:44
   44	    cprintf("H%x Wo%s", 57616, &i);
   ```

   Check `vprintfmt` in `printfmt.c`.

   For `case 'x'`, the hex representation of 57616 is `0xe110`. For `case 's'`, `0x00646c72` would be interpreted as 4 bytes. `0x00` is `NUL` in ASCII, `0x64` is `d` in ASCII, `0x6c` is `l` in ASCII, `0x72` is `r` in ASCII. So the output is `He110 World`. 

   If the x86 is big-endian, then `i` should be `0x726c6400`.

5. Undefined behavior. The next value is read from the stack.



### The Stack

> **Exercise 9.** Determine where the kernel initializes its stack, and exactly where in memory its stack is located. How does the kernel reserve space for its stack? And at which "end" of this reserved area is the stack pointer initialized to point to?

We know for xv6, the boot loader set stack to grow from `0x7c00` towards `0x0000` (check `xv6-public/bootasm.S`). This's the same for JOS, as in `boot/boot.S`. However, this's stack used by boot loader, not kernel. 

Similarly to boot loader `boot.S`, `entry.S` sets up stack before jumping to C code:

```assembly
relocated:
	# Clear the frame pointer register (EBP)
	# so that once we get into debugging C code,
	# stack backtraces will be terminated properly.
	movl	$0x0,%ebp			# nuke frame pointer

	# Set the stack pointer
	movl	$(bootstacktop),%esp

	# now to C code
	call	i386_init
```

From previous exercise, we know the kernel code is loaded at `0x10000c`. 

```
(gdb) x/10i 0x10000c
=> 0x10000c:	movw   $0x1234,0x472
   0x100015:	mov    $0x112000,%eax
   0x10001a:	mov    %eax,%cr3
   0x10001d:	mov    %cr0,%eax
   0x100020:	or     $0x80010001,%eax
   0x100025:	mov    %eax,%cr0
   0x100028:	mov    $0xf010002f,%eax
   0x10002d:	jmp    *%eax
   0x10002f:	mov    $0x0,%ebp			# Initialize %ebp with 0
   0x100034:	mov    $0xf0110000,%esp 	# Initialize %esp to 0xf0110000
```

So the stack is located at virtual memory address `0xf0110000`, physical memory address `0x00110000`.  The stack space is reserved manually in `entry.S`:

```assembly
.data
###################################################################
# boot stack
###################################################################
	.p2align	PGSHIFT		# force page alignment
	.globl		bootstack
bootstack:
	.space		KSTKSIZE
	.globl		bootstacktop   
bootstacktop:
```

As we know, memory is located from lower address to higher address, the stack space looks like: 

```
+-------------------+ <-- bootstacktop
|                   |
|	  KSTKSIZE      |
|                   |
+-------------------+ <-- bootstack
```

and the stack would grow from higher address towards lower address. We know `KSTKSIZE = (8 * PGSIZE) = 8 * 4096 = 2 ^ 15`  bytes. The stack pointer is initialized to `bootstacktop`, `0xf0110000`. 

The x86 %esp points to the lowest location on the stack that's currently in use. The %ebp is used by software convention. C function prologue pushes caller's base pointer onto the stack, and copies the current %esp into %ebp. This allows tracing function call using chain of saved %ebp. 

> **Exercise 10.** To become familiar with the C calling conventions on the x86, find the address of the `test_backtrace` function in `obj/kern/kernel.asm`, set a breakpoint there, and examine what happens each time it gets called after the kernel starts. How many 32-bit words does each recursive nesting level of `test_backtrace` push on the stack, and what are those words?

```
(gdb) break test_backtrace 
Breakpoint 2 at 0xf0100040: file kern/init.c, line 13.
(gdb) disas test_backtrace 
Dump of assembler code for function test_backtrace:
=> 0xf0100040 <+0>:	push   %ebp				# save caller's ebp
   0xf0100041 <+1>:	mov    %esp,%ebp		# update ebp
   0xf0100043 <+3>:	push   %esi				# push callee-saved
   0xf0100044 <+4>:	push   %ebx				# push callee-saved
   0xf0100045 <+5>:	call   0xf01001bc <__x86.get_pc_thunk.bx>
   0xf010004a <+10>:	add    $0x112be,%ebx
   0xf0100050 <+16>:	mov    0x8(%ebp),%esi
   0xf0100053 <+19>:	sub    $0x8,%esp
   0xf0100056 <+22>:	push   %esi
   0xf0100057 <+23>:	lea    -0xf8e8(%ebx),%eax
   0xf010005d <+29>:	push   %eax
   0xf010005e <+30>:	call   0xf0100a49 <cprintf>
   0xf0100063 <+35>:	add    $0x10,%esp
   0xf0100066 <+38>:	test   %esi,%esi
   0xf0100068 <+40>:	jg     0xf0100095 <test_backtrace+85>
   0xf010006a <+42>:	sub    $0x4,%esp
   ...
```

The 32-bit words pushed are listed in comments.

> **Exercise 11.** Implement the backtrace function as specified above. Use the same format as in the example, since otherwise the grading script will be confused. When you think you have it working right, run make grade to see if its output conforms to what our grading script expects, and fix it if it doesn't. *After* you have handed in your Lab 1 code, you are welcome to change the output format of the backtrace function any way you like.

```c
int mon_backtrace(int argc, char **argv, struct Trapframe *tf) {
	cprintf("Stack backtrace:\n");
	
	uint32_t args[5];
	uint32_t* ebp = (uint32_t*)read_ebp();
	while (ebp != 0) {
		uint32_t returnAddr = *(ebp + 1);
		for (int i = 0; i < 5; ++i) args[i] = *(ebp + 2 + i);

		cprintf("  ebp %08x eip %08x args %08x %08x %08x %08x %08x\n", 
				ebp, returnAddr, args[0], args[1], args[2], args[3], args[4]);

		ebp = (uint32_t*) *ebp;
	}
	return 1;
}
```

> **Exercise 12.** Modify your stack backtrace function to display, for each `eip`, the function name, source file name, and line number corresponding to that `eip`.
>
> In `debuginfo_eip`, where do `__STAB_*` come from? This question has a long answer; to help you to discover the answer, here are some things you might want to do:
>
> - look in the file `kern/kernel.ld` for `__STAB_*`
> - run `objdump -h obj/kern/kernel`
> - run `objdump -G obj/kern/kernel`
> - run `gcc -pipe -nostdinc -O2 -fno-builtin -I. -MD -Wall -Wno-format -DJOS_KERNEL -gstabs -c -S kern/init.c`, and look at `init.s`.
> - see if the bootloader loads the symbol table in memory as part of loading the kernel binary
>
> Complete the implementation of `debuginfo_eip` by inserting the call to `stab_binsearch` to find the line number for an address.
>
> Add a `backtrace` command to the kernel monitor, and extend your implementation of `mon_backtrace` to call `debuginfo_eip` and print a line for each stack frame of the form:
>
> ```
> K> backtrace
> Stack backtrace:
>   ebp f010ff78  eip f01008ae  args 00000001 f010ff8c 00000000 f0110580 00000000
>          kern/monitor.c:143: monitor+106
>   ebp f010ffd8  eip f0100193  args 00000000 00001aac 00000660 00000000 00000000
>          kern/init.c:49: i386_init+59
>   ebp f010fff8  eip f010003d  args 00000000 00000000 0000ffff 10cf9a00 0000ffff
>          kern/entry.S:70: <unknown>+0
> K> 
> ```
>
> Each line gives the file name and line within that file of the stack frame's `eip`, followed by the name of the function and the offset of the `eip` from the first instruction of the function (e.g., `monitor+106` means the return `eip` is 106 bytes past the beginning of `monitor`).
>
> Be sure to print the file and function names on a separate line, to avoid confusing the grading script.
>
> Tip: `printf` format strings provide an easy, albeit obscure, way to print non-null-terminated strings like those in STABS tables. `printf("%.*s", length, string)` prints at most `length` characters of `string`. Take a look at the `printf` man page to find out why this works.
>
> You may find that some functions are missing from the backtrace. For example, you will probably see a call to `monitor()` but not to `runcmd()`. This is because the compiler in-lines some function calls. Other optimizations may cause you to see unexpected line numbers. If you get rid of the `-O2` from `GNUMakefile`, the backtraces may make more sense (but your kernel will run more slowly).

Try to read `kernel.ld` file. The [keyword](https://sourceware.org/binutils/docs/ld/PROVIDE.html) `PROVIDE` defines `__STAB_*`. The [special linker variable](https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_chapter/ld_3.html#SEC10) always contains the current output location counter.

```
$ objdump -h obj/kern/kernel
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00001aa9  f0100000  00100000  00001000  2**4
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .rodata       00000718  f0101ac0  00101ac0  00002ac0  2**5
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  2 .stab         00003d39  f01021d8  001021d8  000031d8  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  3 .stabstr      00001997  f0105f11  00105f11  00006f11  2**0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  4 .data         00009300  f0108000  00108000  00009000  2**12
                  CONTENTS, ALLOC, LOAD, DATA
  ...
```

```c
// monitor.c

struct Eipdebuginfo info;
debuginfo_eip((uintptr_t) returnAddr, &info);
cprintf("        %s:%d: ", info.eip_file, info.eip_line);
cprintf("%.*s", info.eip_fn_namelen, info.eip_fn_name);
cprintf("+%d\n", returnAddr - info.eip_fn_addr);
```

```c
// kdebug.c

// YY: lline and rline has already been set for us.
stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
if (lline <= rline) {
	info->eip_line = stabs[lline].n_desc; 
} else {
	return -1;
}
```

The code in `monitor.c` is easy. For `kdebug.c`, I cannot figured out `stabs[lline].n_desc`. But I think this's fine as I know the idea. 

This achieves full score for Lab1.



## Summary

Part1 talks about physical address space layout and BIOS, which loads boot loader into memory at `0x7c00`.

Part2 talks about how boot loader switches from 16-bit real mode to 32-bit protected mode, and load kernel from disk into physical memory. Make sure you understand `boot/boot.S` and `boot/main.c`. It also mentions link address, load address, ELF format,  basic usage of `objdump`.

Part3 talks about the starting code of kernel. `kern/entry.S` sets up hardware-supported virtual memory, switches to use virtual memory by setting `CR0_PG` flag in %cr0, and jumps to C code at high address. Then we see how formatted printing is implemented and how kernel stack is configured. We familarize ourselves with x86 calling convention by implementing the `backtrace` command.
