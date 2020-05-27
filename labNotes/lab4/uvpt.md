# UVPT

```
+--------10------+-------10-------+---------12----------+
| Page Directory |   Page Table   | Offset within Page  |
|      Index     |      Index     |                     |
+----------------+----------------+---------------------+
 \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
 \---------- PGNUM(la) ----------/
```

A nice conceptual model of the page table is a 2^20-entry array indexable by a physicla page number. The 2-level paging scheme breaks this simple model by fragmenting the giant page table into many page tables and one page directory. In the kernel, we use `pgdir_walk` to look up page table entries by walking down the two-level page table. However, to implement `duppage`, a user-mode function, we cannot call `pgdir_walk` or implement something similar (see Note below). 

**Question:** Given a virtual address, how to get its page table entry in user mode?

UVPT is a nice trick of JOS, exploiting the paging hardware, put bring back the conceptual model of continuous page table. 



## Intuition

We are familar with the normal address translation. We follow three arrows:

1. Follow CR3, we get to the page directory;
2. Use bits at `PDX`, we go to the page table;
3. Use bits at `PTX`, we go to the physical page. Append offset, we get the memory content.

![img](https://pdos.csail.mit.edu/6.828/2018/labs/lab4/pagetables.png) 

However, now our goal is to get the page table entry (one level up).

![img](https://pdos.csail.mit.edu/6.828/2018/labs/lab4/vpt.png)

Consider an index V, such that the V-th entry of the page directory points back to the page directory itself (taking the page directory as a page table). Then we have two special cases:

1. If `PDX = V` and `PTX = V`, following 3 arrows, we get back at the page directory. So UVPD = (V << 22) | (V << 12).
2. If `PDX = V` and `PTX != V`, following 3 arrows arrives at the page table entry. In fact, the 4MB region from address (V << 22) are the page tables. So UVPT = V << 22. 

In JOS, V = 0x3BD, thus `UVPT` is `0xEF40000`. The remaining puzzle is how to set `PTX` for a given virtual address.



## Detail

**Claim**: `uvpt[PGNUM(va)]` gives the PTE of virtual address `va`.

**Argument**:

```c
// memlayout.h
extern volatile pte_t uvpt[];     // VA of "virtual page table"
extern volatile pde_t uvpd[];     // VA of current page directory
```

So `uvpt[PGNUM(va)]` is `uvpt + 4 * PGNUM(va)`, i.e., `uvpt + va >> 10`. 

```
+--------10------+-------10-------+---------12----------+
| Page Directory |   Page Table   | Offset within Page  |
|      Index     |      Index     |                     |
+----------------+----------------+---------------------+
 \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
 \---------- PGNUM(la) ----------/
```

Let A, B, C be the first 10 bits, middle 10 bits, last 12 bits of `va`. Let `va' = uvpt[PGNUM(va)]`.  Thus, we know the first 10 bits of `va'` are V, 0x3BD, the middle 10 bits are A, the last 12 bits are B00 (two appended zeros). Now,

- Follow the first arrow, we arrive at the page directory;
- Follow the second arrow (using V), we arrive at the page directory again;
- Follow the third arrow (using A), we arrive at the page table. Appending the offset (using B00) just accesses the B-th entry in the page table. The reason is that each entry takes 4-byte. We get the PTE of `va`!



## Note

We cannot just implement something similar to `pgdir_walk` in the user space. If we ignore creating new page table in `pgdir_walk`, we get:

```c
pte_t *
pgdir_walk(pde_t *pgdir, const void *va)
{
	pde_t *pde;
	pte_t *pgtab;

	pde = &pgdir[PDX(va)];
	if (*pde & PTE_P) {
		// pgtab exists.
		pgtab = (pte_t *) KADDR(PTE_ADDR(*pde));
		return &pgtab[PTX(va)];
	}
	return NULL;
}
```

However, `KADDR` cannot be used in user mode as it's above `UTOP`. 



### Reference

https://zhuanlan.zhihu.com/p/105779179

https://pdos.csail.mit.edu/6.828/2018/labs/lab4/uvpt.html