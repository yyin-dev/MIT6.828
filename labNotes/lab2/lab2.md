# Lab2: Memory Management

Note: It's clear that Page Table Entry stores Physical Page Number, but notice that Page Directory Entry also stores **physical** address to the page tables.



## Part 1: Physical Page Management

A physical page allocator is needed before implementing the rest of virtual memory. Reason: the page table management code requires allocating physical memory for (1) page directory, and (2) the `pages` array of `struct PageInfo`. 

```c
static void *boot_alloc(uint32_t n);
void mem_init(void);
void page_init(void);
struct PageInfo *page_alloc(int alloc_flags);
void page_free(struct PageInfo *pp);
```

`boot_alloc`: allocates physical memory from the end of the `.bss` section of the kernel. 

`mem_init`: allocates space for kernel data stuctures: (1) `kern_pgdir` and (2)`struct PageInfo *pages`, using `boot_alloc`. Note that the page directory is created, but no page table is created yet.

`page_init`: initialize `pages` and `page_free_list` to record free **physical** memory (virtual memory irrelevant here).

`page_alloc`: allocate a physical page from the free list.

`page_free`: return a physical page back to the free list.

The main execution flow:

```c
void mem_init() {
	Detect the amount of memory the machine has;
	Allocate space for kern_pgdir with boot_alloc();
	Allocate space for pages with boot_alloc();
	Call page_init() to initialize pages and page_free_list;
}

void page_init() {
	Add free physical memory to pages and page_free_list;
}
```



## Part2: Virtual Memory

[Segmentation and paging mechanism](https://github.com/yinfredyue/MIT6.828/blob/master/labNotes/lab2/IntelManualReading%20-%20Chapter%205.md). In `boot/boot.S`, segmentation is effectively disabled by setting all segment base addresses to 0 and limits to 0xFFFFFFFF. JOS uses two-level page table. 

Before entering kernel (Lab1), a simple page table maps both virtual address `[0x00000000, 0x00400000]` and virtual address `[0xf0000000, 0xf0400000]` to physical address `[0x00000000, 0x00400000]`. In Lab2, JOS maps the first 256MB of physical memory (this is all physical memory that JOS will use) starting at address 0xF00000000 (this contains the 4MB simple mapping in Lab1) and some other regions. 

The bootloader switched from 16-bit mode to 32-bit protected mode. After that, there's no way to directly use a linear or physical address. *All* memory references are interpreted as virtual addresses and translated by MMU (so all C pointers are virtual addresses). It's meaningless to dereference a physical address now, as MMU translates all addresses.

The JOS kernel may need to find a physical address given a virtual address. Kernel global variables and memory allocated by `boot_alloc` is in the region where the kernel is loaded, starting at 0xF0000000. This is the same address where we mapped ALL physical memory. Thus, to turn a virtual address in this region into a physical address, the kernel can simply subtract 0xF0000000. Use `PADDR(va)` and `KADDR(pa)`.

One physical page can be mapped at multiple virtual address (or in the address spaces of multiple environments). The `PageInfo` struct maintains a `pp_ref` as the physical page reference count. The value should equal to the number of times the physical page appears *below* `UTOP` in all page tables (the mappings above `UTOP` are mostly set up at boot time by the kernel and should never be freed, so there's no need to reference count them). 

Part 1 sets up data structures (`kern_pgdir` and `pages`), Part 2 starts implementing page table management. 

```c
pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);
int page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm);
struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void page_remove(pde_t *pgdir, void *va)
```

`pgdir_walk`: search the page directory and return a pointer to the PTE.

`boot_map_region`: maps an entire region of physical memory to virtual memory. Used to set up "static" mappings above `UTOP` for the kernel. Thus, it should NOT change the reference count.

`page_lookup`: return the corresponding `struct PageInfo*` of a **virtual address**.

`page_remove`: unmap the page at **virtual address**. 

`page_insert`: map a physical page at a **virtual address**. 

In `mem_init`, `lcr3` is called to switch from `entry_pgdir` to `kern_pgdir`. 



## Part 3: Kernel Address Space

JOS divides CPU's 32-bit address space into two parts. User environments (processes) controls the lower part, while the kernel maintains complete control over the upper half. The dividing line is defined arbitrarily by `ULIM`, reserving approximately 256MB of virtual address space for kernel. Refer to `inc/memlayout.h`.

As in `inc/memlayout.h`, kernel and user memory are both present in each environment's address space, we must set permission bits in page tables properly. Otherwise, bugs in user code can overwrite kernel data. 

For `[ULIM, 0xFFFFFFFF]`, the user environment has no permission, while kernel has full permission. For `[UTOP, ULIM)`, both the kernel and user environment can read but not write. This range is used to expose certain kernel data structures read-only to the user environment. For `[0, UTOP)`, the address space is for the user environment to use; the user environment will set permissions for accessing this memory.

In this part, we set up address space above `UTOP`: the kernel part of the address space. 

In `mem_init`, use `boot_map_region` to map `pages` at `UPAGES`, kernel stack at `KSTACKTOP-KSTKSIZE`, and 256MB of physical memory at `KERNBASE`. Because of this, JOS can use at most 256MB of physical memory!

Full execution flow of `mem_init`:

```c
void mem_init() {
	Detect the amount of memory the machine has;
	Allocate space for kern_pgdir with boot_alloc();
	Allocate space for pages with boot_alloc();
	Call page_init() to initialize pages and page_free_list;
    boot_map_region pages at UPAGES;
    boot_map_region kernel stack at KSTACKTOP-KSTKSIZE;
    boot_map_region all physical memory at KERNBASE;
}
```





## Summary
Part 1 allocates space for data structures (`kern_pgdir`, `pages`, and `page_free_list`) and provides functions to manage physical memory, mainly by manipulating two data structures: `pages` and `page_free_list`.  

Part 2 provides functions to manage virtual memory, by manipulating physical memory (using functions in Part 1) and page table.

Part 3 exposes several mappings above `UTOP` (so that user can read some of kernel's data structure), sets up kernel stack, and maps physical memory into kernel's address space. The virtual memory system is set up now. 



## Q & A

- After paging is enabled, how does the kernel access the page directory `kern_pgdir`?

    After the bootloader switched from 16-bit mode to 32-bit protected mode, all memory references are interpreted as virtual addresses and get translated by MMU. So the `kern_pgdir` variable represents a virtual address.

    At entering kernel code, paging uses the page table `entry_pgdir`, which maps virtual address range `[KERNBASE, KERNBASE+4MB)` at physical address `[0, 4MB)`. When `kern_pgdir` is allocated by `boot_alloc` in `mem_init`, it is in the range `[KERNBASE, KERNBASE+4MB)`, covered by `entry_pgdir`. Thus, when paging uses `entry_pgdir`, it can access `kern_pgdir` as the mapping exists in `entry_pgdir`. 

    When setting up virtual memory, kernel switches from `entry_pgdir` to `kern_pgdir`. Before switching, kernel must set up mapping in `kern_pgdir` properly. In `mem_init`, the virtual range `[KERNBASE, 2^32)` is mapped at physical range `[0, 2^32 - KERNBASE)` - all physical memory is mapped above `KERNBASE`. Because `kern_pgdir` maps `[KERNBASE, KERNBASE+4MB)` (memory storing kernel code and kernel data structures like `kern_pgdir` and `pages`) the same way as in `entry_pgdir`, After switching from `entry_pgdir` to `kern_pgdir`, the program still executes correctly and `kern_pgdir` can be accessed correctly.

    The switching is done in `mem_init` by `lcr3(PADDR(kern_pgdir));`. The physical address of `kern_pgdir` is stored into %cr3, to be used by the address translation hardware (MMU).




## Lab Questions [x]

1. Assuming that the following JOS kernel code is correct, what type should variable `x` have, `uintptr_t` or `physaddr_t`?

   ```c
   	mystery_t x;
   	char* value = return_a_pointer();
   	*value = 10;
   	x = (mystery_t) value;
   ```

   Answer: `uintptr_t`.

2. What entries (rows) in the page directory have been filled in at this point? What addresses do they map and where do they point? In other words, fill out this table as much as possible:

   | Entry | Base Virtual Address | Points to (logically):                |
   | ----- | -------------------- | ------------------------------------- |
   | 1023  | ?                    | Page table for top 4MB of phys memory |
   | 1022  | ?                    | ?                                     |
   | .     | ?                    | ?                                     |
   | .     | ?                    | ?                                     |
   | .     | ?                    | ?                                     |
   | 2     | 0x00800000           | ?                                     |
   | 1     | 0x00400000           | ?                                     |
   | 0     | 0x00000000           | [see next question]                   |

   Answer:

   Generally, entry x in the page directory has base virtual address of `0x00400000 * x`. 

   We know `KERNBASE = 0xF0000000` and VA range `[KERNBASE, 2^32)` maps to PA range `[0, 2^32 - KERNBASE)`. The first PDE for the range is of index `PDX(KERNBASE)`, which is 960. Thus, we know `[960, 1023]`, in total 64 PDE are for remapped physical memory.

   VA range `[KSTACKTOP - KSTKSIZE, KSTACKTOP)` is mapped as kernel stack. We know `KSTACKTOP = 0xF0000000` and `KSTACKTOP - KSTKSIZE = 0xEFFF8000`. The first PDE for the range is of index `PDX(KSTACKTOP - KSTKSIZE)`, which is 959. So we know PDE 959 points to the kernel stack. 

   Similarly, this can be calculated for `UPAGES`. 

3. To avoid malicious user code from corrupting the kernel. In page tables, the kernel part of memory is marked as inaccessible for user. The fields in the page table/directory entry. If violated, exception is generated. 

4. `KERNBASE` is `0xF0000000`. So maximum amount is `0xFFFFFFFF - 0xF0000000 = 256MB`.

5. The overhead is page directory and page table. One page directory and 1024 page tables take 1025 * 4KB = 4MB + 4KB = 4050KB.

   From [here](https://zhuanlan.zhihu.com/p/41871340), notice that `pages` is also a memory overhead.

6. The indirect jump

   ```assembly
   mov	$relocated, %eax
   jmp	*%eax
   ```

   causes transitioning to running above `KERNBASE`. We can continue executing at a low EIP between when paging is enabled and when we begin executing at an EIP above `KERNBASE`. The reason is that in `entrypgdir.c`, the same 4MB physical memry range (refer to Lab1) is mapped at both high address and low address. Thus, using this page table, we can still execute at low address. 

   We need to transition to high address since after the real page table is used in `pmap.c`, the low virtual address wouldn't be available.

