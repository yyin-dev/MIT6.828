## PC hardware

- The motherboard contains CPU, memory chips, graphical chips, I/O controller chips, and buses through which the chips communicate.

- Registers provided by modern x86 machines

  - General-purpose registers: ecx, edx, edi, esi, ebp, esp
  - Program counter: eip
  - 80-bit floating point registers
  - Control registers: cr0, cr2, cr3, cr4
  - Debug registers: dr0, dr1, dr2, dr3
  - Segment registers: cs, ds, es, fs, gs, ss
  - Global&Local descriptor table: gdtr, ldtr

  Control registers and segment registers are important to any OS. Floating-point registers and debug registers are less interesting and not used by xv6.

  Usually, x86 processors hide the cache from the OS. So we can think of the processor as having 2 kinds of storage: registers and memory. 

- I/O device

  In modern x86, memory-mapped I/O is used. Devices have fixed memory addresses and the processor communicates with the device by reading/writing values at the addresses.

  Old `in` and `out` instructions still exist. x86 uses them for IDE disk controller.  
  
  

## The boot loader

When an x86 PC goots, it executes BIOS (Basic Input/Ouput System). BIOS is stored in non-violatile memory on the motherboard. BIOS transfers control to code loaded from the *boot sector*, the first 512-byte sector of the *boot disk*. The boot sector contains the *boot loader*: instructions that load the OS kernel into memory. 

The BIOS loads the boot sector from disk at memory address `0x7c00` and jumps to that address. When the boot loader executes, the CPU simulates Intel 8088. The boot loader puts the CPU in a more modern mode, load the xv6 kernel from disk into memory, and transfer control to the kernel. Two files involved: `bootasm.S` and `bootmain.c`. 