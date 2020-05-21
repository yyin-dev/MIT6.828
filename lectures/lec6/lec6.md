# Lec6

- Xv6 address space

  ```
  0xFE000000:0x00000000 -- more memory-mapped devices
  ?         :0x8E000000 -- 224 MB of DRAM mapped here
  0x80100000:?          -- kernel instructions/data
  0x80000000:0x80100000 -- map low 1MB devices (for kernel)
  0x00000000:0x80000000 -- user addresses below KERNBASE
  ```

  ? means not fixed. `[0x80000000, 0x8E000000]` is 224MB.

  ```
  +-------------------+ <- 0x00000000
  |	Mem-mapped I/O  |
  +-------------------+ <- 0xFE000000
  |					|
  |	Unused			|
  |					|
  +-------------------+ <- 0x8E000000
  | Mapped DRAM		|
  |					|
  +-------------------+ <- ?
  | Kernel text&data  |
  +-------------------+ <- 0x80100000
  |	Low 1MB	Devices |
  +-------------------+ <- 0x80000000
  |					|
  |					|
  |	User			|
  |	Memory			|
  |	Space			|
  |					|
  |					|
  |					|
  |					|
  +-------------------+ <- 0x00000000
  ```

  Note that user pages are mapped **twice** at virtual space: (1) in upper half; (2) in lower half.

- Advantage of using this mapping
  - user virtual addresses start at zero
        of course user va 0 maps to different pa for each process
  - 2GB for user heap to grow contiguously
        but needn't have contiguous phys mem -- no fragmentation problem
  - both kernel and user mapped -- easy to switch for syscall, interrupt
      kernel mapped at same place for all processes, eases switching between processes
  - easy for kernel to r/w user memory
        using user addresses, e.g. sys call arguments
  - easy for kernel to r/w physical memory
        pa x mapped at va x+0x80000000