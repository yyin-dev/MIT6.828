# Lab2: Memory Management

It's useful to have some idea about the structure of this lab. 

It's clear that Page Table Entry stores Physical Page Number, but notice that Page Directory Entry also stores **physical** address to the page tables.

## Part 1: Physical Page Management

A physical page allocator is needed before implementing the rest of virtual memory. Reason: the page table management code requires allocating physical memory for (1) page tables, and (2) the `pages` array of `struct PageInfo`. 

```c
static void *boot_alloc(uint32_t n);
void mem_init(void);
void page_init(void);
struct PageInfo *page_alloc(int alloc_flags);
void page_free(struct PageInfo *pp);
```

`boot_alloc`: allocates physical memory from the end of the `.bss` section of the kernel. 

`mem_init`: allocates space for the `pages` array of `struct PageInfo`.

`page_init`: initialize `pages` and `page_free_list`, determining available physical memory.

`page_alloc`: allocate a physical page from the free list.

`page_free`: return a physical page back to the free list.



## Part2: Virtual Memory

Part 1 sets up data structures (page tables and `pages`), Part 2 can start implementing page table management. 

```c
pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);
void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);
int page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm);
struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void page_remove(pde_t *pgdir, void *va)
```

`pgdir_walk`: search the page tables and return a pointer to the PTE.

`boot_map_region`: maps an entire region of physical memory to virtual memory. 

`page_lookup`: return the corresponding `struct PageInfo*` of a **virtual address**.

`page_remove`: unmap the page at **virtual address**. 

`page_insert`: map a physical page at a **virtual address**. 



## Part 3: Kernel Address Space

Add code in `mem_init` to map `pages` at `UPAGES`, kernel stack at `KSTACKTOP-KSTKSIZE`, and 256MB of physical memory at `KERNBASE`. Basically three calls to `boot_map_region`.



## Lab Questions

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

